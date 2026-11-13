import ReactDOM from "react-dom/client";
import { App } from "./App";
import { ErrorBoundary } from "./components/ErrorBoundary";
import { boot } from "./bootstrap";
import { applyAppTheme } from "./theme/apply";
import "./styles.css";

// The shell passes the persisted theme id in boot info so the first frame is
// rendered with the user's theme instead of flashing the dark default.
applyAppTheme(boot.theme);

const root = document.getElementById("root");
if (!root) {
  throw new Error("Anvil root element is missing");
}

ReactDOM.createRoot(root).render(
  <ErrorBoundary>
    <App />
  </ErrorBoundary>,
);
