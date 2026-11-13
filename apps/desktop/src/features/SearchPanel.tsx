import { useEffect, useState } from "react";
import { SearchIcon } from "../components/Icons";
import { useEditorStore } from "../stores/editor";
import { useWorkspaceStore } from "../stores/workspace";
import type { SearchResult } from "../ipc/types";
import { useI18n } from "../i18n";

export function SearchPanel() {
  const { t } = useI18n();
  const root = useWorkspaceStore((state) => state.root);
  const searchFiles = useWorkspaceStore((state) => state.searchFiles);
  const [query, setQuery] = useState("");
  const [results, setResults] = useState<SearchResult[]>([]);
  const [loading, setLoading] = useState(false);
  const [searched, setSearched] = useState(false);

  useEffect(() => {
    if (!root) {
      setResults([]);
      return;
    }
    const trimmed = query.trim();
    if (trimmed.length === 0) {
      setResults([]);
      setSearched(false);
      setLoading(false);
      return;
    }
    let cancelled = false;
    setLoading(true);
    const timer = window.setTimeout(() => {
      void searchFiles(trimmed, 200).then((found) => {
        if (cancelled) return;
        setResults(found);
        setLoading(false);
        setSearched(true);
      });
    }, 180);
    return () => {
      cancelled = true;
      window.clearTimeout(timer);
    };
  }, [query, root, searchFiles]);

  if (!root) {
    return (
      <div className="sidebar-panel">
        <div className="sidebar-header">{t("search.title")}</div>
        <div className="sidebar-empty">
          <p>{t("search.openFolder")}</p>
        </div>
      </div>
    );
  }

  return (
    <div className="sidebar-panel">
      <div className="sidebar-header">{t("search.title")}</div>
      <div className="search-row">
        <SearchIcon size={14} className="search-input-icon" />
        <input
          type="search"
          value={query}
          placeholder={t("search.placeholder")}
          aria-label={t("search.inputLabel")}
          autoFocus
          onChange={(event) => setQuery(event.target.value)}
          onKeyDown={(event) => {
            if (event.key === "Enter" && results[0]) {
              void useEditorStore.getState().openFile(results[0].path);
            }
          }}
        />
      </div>
      <div className="search-note">
        {t("search.note")}
      </div>
      <div className="search-results" aria-live="polite">
        {loading ? <div className="tree-empty">{t("search.searching")}</div> : null}
        {!loading && searched && results.length === 0 ? (
          <div className="tree-empty">{t("search.noMatch", { query: query.trim() })}</div>
        ) : null}
        {results.map((result) => (
          <button
            type="button"
            key={result.path}
            className="search-result"
            onClick={() => void useEditorStore.getState().openFile(result.path)}
            title={result.path}
          >
            <span className="search-result-name">{result.name}</span>
            <span className="search-result-path">{result.path}</span>
          </button>
        ))}
      </div>
    </div>
  );
}
