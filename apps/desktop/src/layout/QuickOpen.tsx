import { useEffect, useMemo, useRef, useState } from "react";
import { useEditorStore } from "../stores/editor";
import { useUiStore } from "../stores/ui";
import { useWorkspaceStore } from "../stores/workspace";
import type { SearchResult } from "../ipc/types";

export function QuickOpen() {
  const open = useUiStore((state) => state.quickOpenOpen);
  const setOpen = useUiStore((state) => state.setQuickOpenOpen);
  const root = useWorkspaceStore((state) => state.root);
  const tabs = useEditorStore((state) => state.tabs);
  const [query, setQuery] = useState("");
  const [results, setResults] = useState<SearchResult[]>([]);
  const [activeIndex, setActiveIndex] = useState(0);
  const [loading, setLoading] = useState(false);
  const inputRef = useRef<HTMLInputElement | null>(null);
  const listRef = useRef<HTMLUListElement | null>(null);

  const openTabs = useMemo(
    () => tabs.map((tab) => ({ path: tab.path, name: tab.name })),
    [tabs],
  );

  useEffect(() => {
    if (!open) return;
    setQuery("");
    setActiveIndex(0);
    window.setTimeout(() => inputRef.current?.focus(), 0);
  }, [open]);

  useEffect(() => {
    if (!open || !root) {
      setResults([]);
      return;
    }
    const trimmed = query.trim();
    if (trimmed.length === 0) {
      setResults(openTabs);
      setLoading(false);
      return;
    }
    let cancelled = false;
    setLoading(true);
    const timer = window.setTimeout(() => {
      void useWorkspaceStore
        .getState()
        .searchFiles(trimmed, 100)
        .then((found) => {
          if (cancelled) return;
          setResults(found);
          setActiveIndex(0);
          setLoading(false);
        });
    }, 150);
    return () => {
      cancelled = true;
      window.clearTimeout(timer);
    };
  }, [open, query, root, openTabs]);

  useEffect(() => {
    if (!open) return;
    const item = listRef.current?.children[activeIndex];
    if (item instanceof HTMLElement) item.scrollIntoView({ block: "nearest" });
  }, [activeIndex, open]);

  if (!open) return null;

  const choose = (result: SearchResult | undefined) => {
    if (!result) return;
    setOpen(false);
    void useEditorStore.getState().openFile(result.path);
  };

  return (
    <div
      className="cmd-overlay"
      onMouseDown={(event) => {
        if (event.target === event.currentTarget) setOpen(false);
      }}
    >
      <div className="cmd-palette" role="dialog" aria-modal="true" aria-label="Go to file">
        <input
          ref={inputRef}
          className="cmd-input"
          value={query}
          placeholder={root ? "Search file names…" : "Open a folder first"}
          aria-label="File name"
          role="combobox"
          aria-expanded="true"
          aria-controls="anvil-quick-open-list"
          onChange={(event) => setQuery(event.target.value)}
          onKeyDown={(event) => {
            if (event.key === "ArrowDown") {
              event.preventDefault();
              setActiveIndex((index) => Math.min(index + 1, results.length - 1));
            } else if (event.key === "ArrowUp") {
              event.preventDefault();
              setActiveIndex((index) => Math.max(index - 1, 0));
            } else if (event.key === "Enter") {
              event.preventDefault();
              choose(results[activeIndex]);
            } else if (event.key === "Escape") {
              event.preventDefault();
              setOpen(false);
            }
          }}
        />
        <ul className="cmd-list" id="anvil-quick-open-list" role="listbox" ref={listRef}>
          {!root ? (
            <li className="cmd-empty">No folder is open.</li>
          ) : loading ? (
            <li className="cmd-empty">Searching…</li>
          ) : results.length === 0 ? (
            <li className="cmd-empty">
              {query.trim() ? "No matching files." : "No open editors. Type a file name."}
            </li>
          ) : (
            results.map((result, index) => (
              <li
                key={result.path}
                role="option"
                aria-selected={index === activeIndex}
                className={index === activeIndex ? "active" : ""}
                onMouseEnter={() => setActiveIndex(index)}
                onClick={() => choose(result)}
              >
                <span className="cmd-title">{result.name}</span>
                <span className="cmd-path">{result.path}</span>
              </li>
            ))
          )}
        </ul>
      </div>
    </div>
  );
}
