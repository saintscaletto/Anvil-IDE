import { useEffect, useMemo, useRef, useState } from "react";
import { commands, type Command } from "../commands";
import { useUiStore } from "../stores/ui";

function score(command: Command, query: string): number {
  const normalized = query.trim().toLowerCase();
  if (!normalized) return 1;
  const title = command.title.toLowerCase();
  const category = command.category.toLowerCase();
  if (title.startsWith(normalized)) return 100;
  const titleIndex = title.indexOf(normalized);
  if (titleIndex >= 0) return 80 - titleIndex;
  const words = normalized.split(/\s+/).filter(Boolean);
  if (words.every((word) => title.includes(word) || category.includes(word))) return 50;
  let cursor = 0;
  for (const character of title) {
    if (character === normalized[cursor]) cursor += 1;
    if (cursor === normalized.length) return 30;
  }
  return 0;
}

export function CommandPalette() {
  const open = useUiStore((state) => state.paletteOpen);
  const setOpen = useUiStore((state) => state.setPaletteOpen);
  const [query, setQuery] = useState("");
  const [activeIndex, setActiveIndex] = useState(0);
  const inputRef = useRef<HTMLInputElement | null>(null);
  const listRef = useRef<HTMLUListElement | null>(null);

  const filtered = useMemo(
    () =>
      commands
        .map((command) => ({ command, score: score(command, query) }))
        .filter((item) => item.score > 0)
        .sort((a, b) => b.score - a.score)
        .map((item) => item.command)
        .slice(0, 60),
    [query],
  );

  useEffect(() => {
    if (!open) return;
    setQuery("");
    setActiveIndex(0);
    window.setTimeout(() => inputRef.current?.focus(), 0);
  }, [open]);

  useEffect(() => {
    if (!open) return;
    const item = listRef.current?.children[activeIndex];
    if (item instanceof HTMLElement) item.scrollIntoView({ block: "nearest" });
  }, [activeIndex, open]);

  if (!open) return null;

  const run = (command: Command) => {
    setOpen(false);
    try {
      void Promise.resolve(command.run()).catch((error: unknown) => {
        useUiStore
          .getState()
          .toast("error", error instanceof Error ? error.message : "Command failed");
      });
    } catch (error) {
      useUiStore
        .getState()
        .toast("error", error instanceof Error ? error.message : "Command failed");
    }
  };

  return (
    <div
      className="cmd-overlay"
      onMouseDown={(event) => {
        if (event.target === event.currentTarget) setOpen(false);
      }}
    >
      <div className="cmd-palette" role="dialog" aria-modal="true" aria-label="Command palette">
        <input
          ref={inputRef}
          className="cmd-input"
          value={query}
          placeholder="Type a command…"
          aria-label="Command"
          role="combobox"
          aria-expanded="true"
          aria-controls="anvil-command-list"
          onChange={(event) => {
            setQuery(event.target.value);
            setActiveIndex(0);
          }}
          onKeyDown={(event) => {
            if (event.key === "ArrowDown") {
              event.preventDefault();
              setActiveIndex((index) => Math.min(index + 1, filtered.length - 1));
            } else if (event.key === "ArrowUp") {
              event.preventDefault();
              setActiveIndex((index) => Math.max(index - 1, 0));
            } else if (event.key === "Enter") {
              event.preventDefault();
              const command = filtered[activeIndex];
              if (command) run(command);
            } else if (event.key === "Escape") {
              event.preventDefault();
              setOpen(false);
            }
          }}
        />
        <ul className="cmd-list" id="anvil-command-list" role="listbox" ref={listRef}>
          {filtered.length === 0 ? (
            <li className="cmd-empty">No matching commands.</li>
          ) : (
            filtered.map((command, index) => (
              <li
                key={command.id}
                role="option"
                aria-selected={index === activeIndex}
                className={index === activeIndex ? "active" : ""}
                onMouseEnter={() => setActiveIndex(index)}
                onClick={() => run(command)}
              >
                <span className="cmd-category">{command.category}</span>
                <span className="cmd-title">{command.title}</span>
                {command.keybinding ? <kbd>{command.keybinding}</kbd> : null}
              </li>
            ))
          )}
        </ul>
      </div>
    </div>
  );
}
