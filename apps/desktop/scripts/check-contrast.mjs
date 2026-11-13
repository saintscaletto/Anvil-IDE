#!/usr/bin/env node
// Contrast gate for the three Anvil themes. Reads the token blocks from
// styles.css and checks the text/background pairs that carry UI text.
// Run: npm run check:contrast (from apps/desktop).

import { readFileSync } from "node:fs";
import { fileURLToPath } from "node:url";
import { dirname, join } from "node:path";

const here = dirname(fileURLToPath(import.meta.url));
const css = readFileSync(join(here, "..", "src", "styles.css"), "utf8");

const themeBlocks = {};
const blockPattern = /:root(?:\[data-theme="([^"]+)"\])?\s*\{([\s\S]*?)\}/g;
for (const match of css.matchAll(blockPattern)) {
  const name = match[1] ?? "dark";
  const tokens = {};
  for (const token of match[2].matchAll(/--([\w-]+):\s*([^;]+);/g)) {
    tokens[token[1]] = token[2].trim();
  }
  themeBlocks[name] = tokens;
}

function parseHex(value) {
  if (!/^#[0-9a-fA-F]{3,8}$/.test(value)) return null;
  let hex = value.slice(1);
  if (hex.length === 3) hex = [...hex].map((c) => c + c).join("");
  if (hex.length === 8) hex = hex.slice(0, 6);
  if (hex.length !== 6) return null;
  return [0, 2, 4].map((i) => parseInt(hex.slice(i, i + 2), 16) / 255);
}

function luminance([r, g, b]) {
  const linear = [r, g, b].map((c) =>
    c <= 0.03928 ? c / 12.92 : Math.pow((c + 0.055) / 1.055, 2.4),
  );
  return 0.2126 * linear[0] + 0.7152 * linear[1] + 0.0722 * linear[2];
}

function contrast(foreground, background) {
  const a = luminance(foreground);
  const b = luminance(background);
  const [light, dark] = a > b ? [a, b] : [b, a];
  return (light + 0.05) / (dark + 0.05);
}

const pairs = [
  ["text-primary", "bg-primary"],
  ["text-primary", "bg-secondary"],
  ["text-primary", "bg-tertiary"],
  ["text-muted", "bg-primary"],
  ["text-muted", "bg-secondary"],
  ["text-muted", "bg-tertiary"],
  ["accent-text", "bg-primary"],
  ["accent-text", "bg-secondary"],
  ["text-on-accent", "accent-button"],
  ["error-text", "bg-primary"],
  ["error-text", "bg-secondary"],
  ["text-on-error", "error-button"],
  ["warning", "bg-primary"],
  ["success", "bg-primary"],
  ["status-text", "status-bg"],
  ["badge-text", "bg-primary"],
  ["info-text", "info-bg"],
  ["warning", "warning-bg"],
  ["error-text", "error-bg"],
  ["success", "success-bg"],
];

const minimum = 4.5;
let failures = 0;
for (const [themeName, tokens] of Object.entries(themeBlocks)) {
  for (const [fgName, bgName] of pairs) {
    const fg = parseHex(tokens[fgName] ?? "");
    const bg = parseHex(tokens[bgName] ?? "");
    if (!fg || !bg) {
      console.error(`FAIL ${themeName}: missing or non-hex token ${fgName}/${bgName}`);
      failures += 1;
      continue;
    }
    const ratio = contrast(fg, bg);
    if (ratio + 1e-9 < minimum) {
      console.error(
        `FAIL ${themeName}: ${fgName} on ${bgName} = ${ratio.toFixed(2)} (need ${minimum})`,
      );
      failures += 1;
    } else {
      console.log(`ok   ${themeName}: ${fgName} on ${bgName} = ${ratio.toFixed(2)}`);
    }
  }
}

if (failures > 0) {
  console.error(`\n${failures} contrast failure(s).`);
  process.exit(1);
}
console.log(`\nAll ${Object.keys(themeBlocks).length} themes meet WCAG AA (${minimum}:1) for UI text.`);
