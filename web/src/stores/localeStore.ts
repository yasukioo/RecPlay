import { create } from "zustand";

import type { Locale } from "../types";

export const LOCALE_STORAGE_KEY = "recplay-locale";

interface LocaleStoreState {
  locale: Locale;
  setLocale: (locale: Locale) => void;
  toggleLocale: () => void;
}

export function sanitizeLocale(value: string | null | undefined): Locale {
  return value === "zh-CN" ? "zh-CN" : "en-US";
}

export const useLocaleStore = create<LocaleStoreState>((set) => ({
  locale: "en-US",
  setLocale: (locale) => set({ locale }),
  toggleLocale: () =>
    set((current) => ({
      locale: current.locale === "en-US" ? "zh-CN" : "en-US",
    })),
}));
