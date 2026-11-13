import type { LspPosition, LspRange } from "../ipc/types";

const lastSent = new Map<string, string>();

export interface LspChange {
  range: LspRange;
  text: string;
}

function positionAt(content: string, offset: number): LspPosition {
  let line = 0;
  let character = 0;
  const limit = Math.max(0, Math.min(offset, content.length));
  for (let index = 0; index < limit; index += 1) {
    const code = content.charCodeAt(index);
    if (code === 13) {
      if (content.charCodeAt(index + 1) === 10) index += 1;
      line += 1;
      character = 0;
    } else if (code === 10) {
      line += 1;
      character = 0;
    } else {
      character += 1;
    }
  }
  return { line, character };
}

function avoidSplitSurrogate(content: string, offset: number): number {
  if (offset <= 0 || offset >= content.length) return offset;
  const before = content.charCodeAt(offset - 1);
  const after = content.charCodeAt(offset);
  const beforeIsHigh = before >= 0xd800 && before <= 0xdbff;
  const afterIsLow = after >= 0xdc00 && after <= 0xdfff;
  return beforeIsHigh && afterIsLow ? offset - 1 : offset;
}

export function resetTextSync(path: string, content: string): void {
  lastSent.set(path, content);
}

export function forgetTextSync(path: string): void {
  lastSent.delete(path);
}

export function computeTextChange(path: string, nextContent: string): LspChange | null {
  const previous = lastSent.get(path);
  if (previous === undefined) {
    lastSent.set(path, nextContent);
    return null;
  }
  if (previous === nextContent) return null;

  let prefix = 0;
  const maxPrefix = Math.min(previous.length, nextContent.length);
  while (prefix < maxPrefix && previous.charCodeAt(prefix) === nextContent.charCodeAt(prefix)) {
    prefix += 1;
  }
  prefix = avoidSplitSurrogate(previous, prefix);

  let suffix = 0;
  const maxSuffix = Math.min(previous.length - prefix, nextContent.length - prefix);
  while (
    suffix < maxSuffix &&
    previous.charCodeAt(previous.length - 1 - suffix) ===
      nextContent.charCodeAt(nextContent.length - 1 - suffix)
  ) {
    suffix += 1;
  }

  const oldEnd = previous.length - suffix;
  const newEnd = nextContent.length - suffix;
  const change: LspChange = {
    range: {
      start: positionAt(previous, prefix),
      end: positionAt(previous, oldEnd),
    },
    text: nextContent.slice(prefix, newEnd),
  };
  lastSent.set(path, nextContent);
  return change;
}
