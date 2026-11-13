import { useEffect, useMemo, useRef, useState } from "react";
import { commands, type Command } from "../commands";
import { useI18n } from "../i18n";
import { useUiStore } from "../stores/ui";

interface LocalizedCommand {
  command: Command;
  title: string;
  category: string;
}

function score(item: LocalizedCommand, query: string): number {
  const normalized = query.trim().toLowerCase();
  if (!normalized) return 1;
  const title = item.title.toLowerCase();
  const category = item.category.toLowerCase();
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
  const { t } = useI18n();
  const open = useUiStore((state) => state.paletteOpen);
  const setOpen = useUiStore((state) => state.setPaletteOpen);
  const [query, setQuery] = useState("");
  const [activeIndex, setActiveIndex] = useState(0);
  const inputRef = useRef<HTMLInputElement | null>(null);
  const listRef = useRef<HTMLUListElement | null>(null);

  const localized = useMemo(
    () =>
      commands.map((command) => ({
        command,
        title: t(command.titleKey),
        category: t(command.categoryKey),
      })),
    [t],
  );

  const filtered = useMemo(
    () =>
      localized
        .map((item) => ({ item, score: score(item, query) }))
        .filter((entry) => entry.score > 0)
        .sort((a, b) => b.score - a.score)
        .map((entry) => entry.item)
        .slice(0, 60),
    [localized, query],
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
          .toast("error", error instanceof Error ? error.message : t("palette.commandFailed"));
      });
    } catch (error) {
      useUiStore
        .getState()
        .toast("error", error instanceof Error ? error.message : t("palette.commandFailed"));
    }
  };

  return (
    <div
      className="cmd-overlay"
      onMouseDown={(event) => {
        if (event.target === event.currentTarget) setOpen(false);
      }}
    >
      <div className="cmd-palette" role="dialog" aria-modal="true" aria-label={t("palette.dialogLabel")}>
        <input
          ref={inputRef}
          className="cmd-input"
          value={query}
          placeholder={t("palette.placeholder")}
          aria-label={t("palette.inputLabel")}
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
              const selected = filtered[activeIndex];
              if (selected) run(selected.command);
            } else if (event.key === "Escape") {
              event.preventDefault();
              setOpen(false);
            }
          }}
        />
        <ul className="cmd-list" id="anvil-command-list" role="listbox" ref={listRef}>
          {filtered.length === 0 ? (
            <li className="cmd-empty">{t("palette.empty")}</li>
          ) : (
            filtered.map((entry, index) => (
              <li
                key={entry.command.id}
                role="option"
                aria-selected={index === activeIndex}
                className={index === activeIndex ? "active" : ""}
                onMouseEnter={() => setActiveIndex(index)}
                onClick={() => run(entry.command)}
              >
                <span className="cmd-category">{entry.category}</span>
                <span className="cmd-title">{entry.title}</span>
                {entry.command.keybinding ? <kbd>{entry.command.keybinding}</kbd> : null}
              </li>
            ))
          )}
        </ul>
      </div>
    </div>
  );
}
