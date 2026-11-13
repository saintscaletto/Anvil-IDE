import ReactDOM from "react-dom/client";
import { App } from "./App";
import { ErrorBoundary } from "./components/ErrorBoundary";
import { boot } from "./bootstrap";
import { applyAppTheme } from "./theme/apply";
import { resolveLocale, setRuntimeLocale } from "./i18n";
import "./styles.css";

// The shell passes the persisted theme id and system locale in boot info so the
// first frame uses the right theme and imperative error paths have a locale.
applyAppTheme(boot.theme);
setRuntimeLocale(resolveLocale("auto", boot.locale));

const root = document.getElementById("root");
if (!root) {
  throw new Error("Anvil root element is missing");
}

ReactDOM.createRoot(root).render(
  <ErrorBoundary>
    <App />
  </ErrorBoundary>,
);
