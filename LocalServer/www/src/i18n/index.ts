import { create } from 'zustand';
import { messages } from './messages.generated';

export type Locale = 'en' | 'zh-CN';

const STORAGE_KEY = 'pmc.locale';

export type TranslateFn = (key: string, vars?: Record<string, string | number>) => string;
export type LabelFn = (t: TranslateFn) => string;

export const LOCALES: { id: Locale; label: LabelFn }[] = [
  { id: 'en', label: (t) => t('English') },
  { id: 'zh-CN', label: (t) => t('Simplified Chinese') },
];

function readStoredLocale(): Locale | null {
  try {
    const v = localStorage.getItem(STORAGE_KEY);
    if (v === 'en' || v === 'zh-CN') return v;
  } catch {
    /* ignore */
  }
  return null;
}

export function detectLocale(): Locale {
  const stored = readStoredLocale();
  if (stored) return stored;
  if (typeof navigator !== 'undefined' && navigator.language.toLowerCase().startsWith('zh')) {
    return 'zh-CN';
  }
  return 'en';
}

function persistLocale(locale: Locale) {
  try {
    localStorage.setItem(STORAGE_KEY, locale);
  } catch {
    /* ignore */
  }
  if (typeof document !== 'undefined') {
    document.documentElement.lang = locale === 'zh-CN' ? 'zh-CN' : 'en';
  }
}

export function interpolate(template: string, vars?: Record<string, string | number>): string {
  if (!vars) return template;
  return template.replace(/\{(\w+)\}/g, (_, name: string) =>
    Object.prototype.hasOwnProperty.call(vars, name) ? String(vars[name]) : `{${name}}`,
  );
}

export function translate(locale: Locale, key: string, vars?: Record<string, string | number>): string {
  const table = locale === 'en' ? undefined : messages[locale];
  return interpolate(table?.[key] || key, vars);
}

interface I18nState {
  locale: Locale;
  setLocale: (locale: Locale) => void;
}

const initial = detectLocale();
if (typeof document !== 'undefined') {
  document.documentElement.lang = initial === 'zh-CN' ? 'zh-CN' : 'en';
}

export const useI18nStore = create<I18nState>((set) => ({
  locale: initial,
  setLocale: (locale) => {
    persistLocale(locale);
    set({ locale });
  },
}));

export function t(key: string, vars?: Record<string, string | number>): string {
  return translate(useI18nStore.getState().locale, key, vars);
}

export function useT(): TranslateFn {
  const locale = useI18nStore((s) => s.locale);
  return (key: string, vars?: Record<string, string | number>) => translate(locale, key, vars);
}
