#!/usr/bin/env node
// Fails on obvious untranslated UI text: literal JSX text nodes and literal
// aria-label/title/placeholder attributes. Product names, shortcuts, and
// technical values are allowlisted. Run: npm run check:i18n.

import { readdirSync, readFileSync, statSync } from "node:fs";
import { fileURLToPath } from "node:url";
import { dirname, join, relative } from "node:path";

const here = dirname(fileURLToPath(import.meta.url));
const sourceRoot = join(here, "..", "src");

function walk(directory) {
  const files = [];
  for (const entry of readdirSync(directory)) {
    const path = join(directory, entry);
    if (statSync(path).isDirectory()) {
      if (entry === "i18n") continue;
      files.push(...walk(path));
    } else if (entry.endsWith(".tsx")) {
      files.push(path);
    }
  }
  return files;
}

const allowedText = new Set([
  "A",
  "Anvil",
  "IDE",
  "Anvil AI",
  "C++",
  "C",
  "H",
  "Rs",
  "Py",
  "CM",
  "Apache-2.0",
  "UTF-8",
  "LF",
  "CRLF",
  "bubblewrap",
  "clangd or /usr/bin/clangd-18",
  "--background-index",
]);

function allowed(value) {
  const trimmed = value.replace(/\s+/g, " ").trim();
  if (!trimmed) return true;
  if (allowedText.has(trimmed)) return true;
  // Keyboard shortcuts and version chips are intentionally not translated.
  if (/^(Ctrl|Shift|Alt|Meta|F\d+)(\+(Ctrl|Shift|Alt|Meta|Key[A-Z]|F\d+|[A-Z0-9]))*$/.test(trimmed)) {
    return true;
  }
  if (/^v?\d+(\.\d+)*$/.test(trimmed)) return true;
  if (!/[A-Za-z]/.test(trimmed)) return true;
  return false;
}

const failures = [];
for (const file of walk(sourceRoot)) {
  const text = readFileSync(file, "utf8");
  const name = relative(join(here, ".."), file);
  const lines = text.split("\n");
  lines.forEach((line, index) => {
    for (const match of line.matchAll(/>([^<>{}]*[A-Za-z][^<>{}]*)</g)) {
      const value = match[1];
      if (!allowed(value)) {
        failures.push(`${name}:${index + 1}: JSX text "${value.trim()}"`);
      }
    }
    for (const match of line.matchAll(
      /(?:aria-label|title|placeholder)="([^"]*[A-Za-z][^"]*)"/g,
    )) {
      const value = match[1];
      if (!allowed(value)) {
        failures.push(`${name}:${index + 1}: attribute "${value}"`);
      }
    }
  });
}

if (failures.length > 0) {
  console.error("Untranslated UI text found:");
  for (const failure of failures) console.error(`  ${failure}`);
  process.exit(1);
}
console.log("No untranslated JSX text or literal UI attributes found.");
