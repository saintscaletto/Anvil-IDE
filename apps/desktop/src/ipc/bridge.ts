import { nativeBridgeAvailable } from "../bootstrap";
import { t } from "../i18n";

export class AnvilError extends Error {
  readonly code: number;
  readonly data: unknown;

  constructor(code: number, message: string, data?: unknown) {
    super(message);
    this.name = "AnvilError";
    this.code = code;
    this.data = data;
  }
}

type Listener = (params: unknown) => void;

interface PendingRequest {
  method: string;
  resolve: (value: unknown) => void;
  reject: (reason: AnvilError) => void;
  timer: number;
}

interface WireError {
  code: number;
  message: string;
  data?: unknown;
}

const isRecord = (value: unknown): value is Record<string, unknown> =>
  typeof value === "object" && value !== null && !Array.isArray(value);

const isWireError = (value: unknown): value is WireError => {
  if (!isRecord(value)) return false;
  return typeof value["code"] === "number" && typeof value["message"] === "string";
};

class Bridge {
  private nextId = 1;
  private readonly pending = new Map<number, PendingRequest>();
  private readonly listeners = new Map<string, Set<Listener>>();
  private readonly bufferedEvents = new Map<string, unknown[]>();

  constructor() {
    window.__anvilReceive = (message: unknown) => this.receive(message);
  }

  get available(): boolean {
    return nativeBridgeAvailable;
  }

  request<T>(method: string, params: Record<string, unknown> = {}, timeoutMs = 20_000): Promise<T> {
    if (!this.available) {
      return Promise.reject(
        new AnvilError(-32000, t("error.bridgeUnavailable")),
      );
    }
    const id = this.nextId++;
    return new Promise<T>((resolve, reject) => {
      const timer = window.setTimeout(() => {
        this.pending.delete(id);
        this.notify("request/cancel", { targetId: id });
        reject(new AnvilError(-32011, t("error.requestTimeout", { method })));
      }, timeoutMs);
      this.pending.set(id, {
        method,
        resolve: (value) => resolve(value as T),
        reject,
        timer,
      });
      try {
        window.webkit?.messageHandlers?.anvil?.postMessage({
          type: "request",
          id,
          method,
          params,
          apiVersion: 1,
        });
      } catch (error) {
        window.clearTimeout(timer);
        this.pending.delete(id);
        reject(
          new AnvilError(
            -32000,
            error instanceof Error ? error.message : t("error.postMessage"),
          ),
        );
      }
    });
  }

  notify(method: string, params: Record<string, unknown> = {}): void {
    if (!this.available) return;
    try {
      window.webkit?.messageHandlers?.anvil?.postMessage({
        type: "notification",
        method,
        params,
        apiVersion: 1,
      });
    } catch {
      // The shell may have gone away while the window is closing.
    }
  }

  on(method: string, listener: Listener): () => void {
    let set = this.listeners.get(method);
    if (!set) {
      set = new Set();
      this.listeners.set(method, set);
    }
    set.add(listener);
    const buffered = this.bufferedEvents.get(method);
    if (buffered) {
      this.bufferedEvents.delete(method);
      for (const params of buffered) listener(params);
    }
    return () => {
      set?.delete(listener);
    };
  }

  private receive(message: unknown): void {
    if (!isRecord(message)) return;
    const type = message["type"];
    if (type === "response") {
      const id = message["id"];
      if (typeof id !== "number") return;
      const pending = this.pending.get(id);
      if (!pending) return;
      this.pending.delete(id);
      window.clearTimeout(pending.timer);
      if (isWireError(message["error"])) {
        const error = message["error"];
        pending.reject(new AnvilError(error.code, error.message, error.data));
        return;
      }
      pending.resolve(message["result"]);
      return;
    }
    if (type !== "event") return;
    const method = message["method"];
    if (typeof method !== "string") return;
    const params = message["params"];
    const set = this.listeners.get(method);
    if (set && set.size > 0) {
      for (const listener of set) listener(params);
      return;
    }
    const queue = this.bufferedEvents.get(method) ?? [];
    if (queue.length < 200) queue.push(params);
    this.bufferedEvents.set(method, queue);
  }
}

export const bridge = new Bridge();
