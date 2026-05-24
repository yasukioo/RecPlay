import { create } from "zustand";

import type { ThemeMode } from "../types";

export const THEME_STORAGE_KEY = "recplay-theme";

interface ThemeStoreState {
  theme: ThemeMode;
  setTheme: (theme: ThemeMode) => void;
  toggleTheme: () => void;
}

export function sanitizeTheme(value: string | null | undefined): ThemeMode {
  return value === "light" ? "light" : "dark";
}

export const useThemeStore = create<ThemeStoreState>((set) => ({
  theme: "dark",
  setTheme: (theme) => set({ theme }),
  toggleTheme: () =>
    set((current) => ({
      theme: current.theme === "dark" ? "light" : "dark",
    })),
}));
