import type { EditorLineHeight, Settings } from "../ipc/types";

// Keep these in sync with the CSS defaults in styles.css. The fallback chain
// always ends with a generic family so a missing font never breaks layout.
export const DEFAULT_UI_FONT =
  '-apple-system, "Segoe UI", Roboto, "Noto Sans", sans-serif';
export const DEFAULT_CODE_FONT =
  '"JetBrains Mono", "Fira Code", Consolas, "Liberation Mono", monospace';

// Conservative mirror of the core validator: family names are data, never CSS.
const FONT_FAMILY_PATTERN = /^[\p{L}\p{N} _.\-+]*$/u;

export function isValidFontFamily(value: string): boolean {
  return value.length <= 128 && FONT_FAMILY_PATTERN.test(value);
}

export function fontStack(family: string, fallback: string): string {
  const trimmed = family.trim();
  if (!trimmed) return fallback;
  if (!isValidFontFamily(trimmed)) return fallback;
  return `"${trimmed}", ${fallback}`;
}

export function uiFontStack(settings: Settings): string {
  return fontStack(settings["ui.fontFamily"], DEFAULT_UI_FONT);
}

export function editorFontStack(settings: Settings): string {
  return fontStack(settings["editor.fontFamily"], DEFAULT_CODE_FONT);
}

export function lineHeightFor(mode: EditorLineHeight, fontSize: number): number {
  const multiplier = mode === "compact" ? 1.2 : mode === "relaxed" ? 1.7 : 1.45;
  return Math.max(1, Math.round(fontSize * multiplier));
}

/**
 * Applies UI and editor font preferences through validated CSS custom
 * properties. A persisted family that fails validation falls back to the
 * built-in stack instead of reaching the CSS parser.
 */
export function applyFonts(settings: Settings): void {
  const root = document.documentElement;
  root.style.setProperty("--font-ui", uiFontStack(settings));
  root.style.setProperty("--font-code", editorFontStack(settings));
  const size = Math.min(18, Math.max(11, Math.round(settings["ui.fontSize"])));
  root.style.setProperty("--ui-font-size", `${size}px`);
}
