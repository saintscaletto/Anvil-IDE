import { createContext, useContext, useMemo, type ReactNode } from "react";
import { boot } from "../bootstrap";
import type { LanguagePreference } from "../ipc/types";
import { en, type MessageKey } from "./en";
import { ru } from "./ru";

export type Locale = "en" | "ru";
export type MessageParams = Record<string, string | number>;

const dictionaries: Record<Locale, Record<MessageKey, string>> = { en, ru };

export function isLocale(value: unknown): value is Locale {
  return value === "en" || value === "ru";
}

/**
 * `auto` follows the normalized system locale passed by the native shell.
 * Only the language subtag matters: ru_RU.UTF-8 and ru-RU both resolve to ru.
 */
export function resolveLocale(
  preference: LanguagePreference,
  systemLocale: string,
): Locale {
  if (preference === "en" || preference === "ru") return preference;
  return /^ru(?:[_-]|$)/i.test(systemLocale) ? "ru" : "en";
}

function format(template: string, params?: MessageParams): string {
  if (!params) return template;
  return template.replace(/\{(\w+)\}/g, (match, name: string) =>
    Object.prototype.hasOwnProperty.call(params, name) ? String(params[name]) : match,
  );
}

export function translate(
  locale: Locale,
  key: MessageKey,
  params?: MessageParams,
): string {
  const template = dictionaries[locale][key] ?? en[key] ?? key;
  return format(template, params);
}

// Global locale for imperative code (actions, stores, error boundary). React
// components use useI18n() so they re-render when the setting changes.
let currentLocale: Locale = resolveLocale("auto", boot.locale);

export function setRuntimeLocale(locale: Locale): void {
  currentLocale = locale;
}

export function getLocale(): Locale {
  return currentLocale;
}

export function t(key: MessageKey, params?: MessageParams): string {
  return translate(currentLocale, key, params);
}

export function countCategory(locale: Locale, count: number): "one" | "few" | "many" {
  if (locale === "ru") {
    const mod10 = count % 10;
    const mod100 = count % 100;
    if (mod10 === 1 && mod100 !== 11) return "one";
    if (mod10 >= 2 && mod10 <= 4 && (mod100 < 12 || mod100 > 14)) return "few";
    return "many";
  }
  return count === 1 ? "one" : "many";
}

export function tCount(
  baseKey: string,
  count: number,
  params?: MessageParams,
): string {
  const locale = getLocale();
  const key = `${baseKey}.${countCategory(locale, count)}` as MessageKey;
  return translate(locale, key, { count, ...params });
}

const I18nContext = createContext<Locale>("en");

export function I18nProvider({
  locale,
  children,
}: {
  locale: Locale;
  children: ReactNode;
}) {
  return <I18nContext.Provider value={locale}>{children}</I18nContext.Provider>;
}

export function useLocale(): Locale {
  return useContext(I18nContext);
}

export function useI18n(): {
  locale: Locale;
  t: (key: MessageKey, params?: MessageParams) => string;
} {
  const locale = useContext(I18nContext);
  const t = useMemo(
    () => (key: MessageKey, params?: MessageParams) => translate(locale, key, params),
    [locale],
  );
  return { locale, t };
}

export type { MessageKey };
