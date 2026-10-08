import { create } from "zustand";

import type { ThemeMode } from "../types";

export const THEME_STORAGE_KEY = "recplay-theme";

interface StorageReader {
  getItem: (key: string) => string | null;
  setItem?: (key: string, value: string) => void;
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
  setTheme: (theme) => {
    persistTheme(theme);
    set({ theme });
  },
  toggleTheme: () =>
    set((current) => {
      const theme = current.theme === "dark" ? "light" : "dark";
      persistTheme(theme);
      return { theme };
    }),
}));

function resolveStorage(): StorageReader | null {
  if (typeof window === "undefined") {
    return null;
  }

  return window.localStorage;
}

function persistTheme(theme: ThemeMode): void {
  resolveStorage()?.setItem?.(THEME_STORAGE_KEY, theme);
}
