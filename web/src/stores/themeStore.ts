import { create } from "zustand";

import type { ThemeMode } from "../types";

export const THEME_STORAGE_KEY = "recplay-theme";

interface StorageReader {
  getItem: (key: string) => string | null;
}

interface ThemeStoreState {
  theme: ThemeMode;
  setTheme: (theme: ThemeMode) => void;
  toggleTheme: () => void;
}

export function sanitizeTheme(value: string | null | undefined): ThemeMode {
  return value === "light" ? "light" : "dark";
}

export function getInitialTheme(storage: StorageReader | null = resolveStorage()): ThemeMode {
  return sanitizeTheme(storage?.getItem(THEME_STORAGE_KEY));
}

export const useThemeStore = create<ThemeStoreState>((set) => ({
  theme: getInitialTheme(),
  setTheme: (theme) => set({ theme }),
  toggleTheme: () =>
    set((current) => ({
      theme: current.theme === "dark" ? "light" : "dark",
    })),
}));

function resolveStorage(): StorageReader | null {
  if (typeof window === "undefined") {
    return null;
  }

  return window.localStorage;
}
