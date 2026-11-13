import type { ThemePreference } from "../ipc/types";

export type AppTheme = "dark" | "light" | "high-contrast";

const APP_THEMES: readonly AppTheme[] = ["dark", "light", "high-contrast"];

export function isThemePreference(value: unknown): value is ThemePreference {
  return value === "auto" || APP_THEMES.includes(value as AppTheme);
}

export function isAppTheme(value: unknown): value is AppTheme {
  return APP_THEMES.includes(value as AppTheme);
}

export function systemTheme(): AppTheme {
  if (typeof window === "undefined" || typeof window.matchMedia !== "function") {
    return "dark";
  }
  return window.matchMedia("(prefers-color-scheme: light)").matches ? "light" : "dark";
}

export function resolveTheme(preference: ThemePreference): AppTheme {
  return preference === "auto" ? systemTheme() : preference;
}

/**
 * Applies the resolved theme to the document. The value is always one of the
 * allowlisted ids from the settings schema, never interpolated user input.
 */
export function applyAppTheme(preference: ThemePreference): AppTheme {
  const resolved = resolveTheme(preference);
  const root = document.documentElement;
  root.dataset.theme = resolved;
  root.style.setProperty("color-scheme", resolved === "light" ? "light" : "dark");
  return resolved;
}

export function currentAppTheme(): AppTheme {
  const value = document.documentElement.dataset.theme;
  return isAppTheme(value) ? value : "dark";
}

export function watchSystemTheme(onChange: () => void): () => void {
  if (typeof window.matchMedia !== "function") return () => undefined;
  const media = window.matchMedia("(prefers-color-scheme: light)");
  const handler = () => onChange();
  media.addEventListener?.("change", handler);
  return () => media.removeEventListener?.("change", handler);
}
