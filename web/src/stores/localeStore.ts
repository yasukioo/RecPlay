import { create } from "zustand";

import type { Locale } from "../types";

export const LOCALE_STORAGE_KEY = "recplay-locale";

interface StorageReader {
  getItem: (key: string) => string | null;
}

interface LocaleStoreState {
  locale: Locale;
  setLocale: (locale: Locale) => void;
  toggleLocale: () => void;
}

export function sanitizeLocale(value: string | null | undefined): Locale {
  return value === "zh-CN" ? "zh-CN" : "en-US";
}

export function getInitialLocale(storage: StorageReader | null = resolveStorage()): Locale {
  return sanitizeLocale(storage?.getItem(LOCALE_STORAGE_KEY));
}

export const useLocaleStore = create<LocaleStoreState>((set) => ({
  locale: getInitialLocale(),
  setLocale: (locale) => set({ locale }),
  toggleLocale: () =>
    set((current) => ({
      locale: current.locale === "en-US" ? "zh-CN" : "en-US",
    })),
}));

function resolveStorage(): StorageReader | null {
  if (typeof window === "undefined") {
    return null;
  }

  return window.localStorage;
}
