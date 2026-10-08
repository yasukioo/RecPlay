import { create } from "zustand";

/**
 * UI / shell state for the DRR-style frontend: which workbench is showing plus
 * the presentation settings the design稿 exposes via its Tweaks/Settings panel.
 * Persisted to localStorage so a reload restores the last mode + settings.
 *
 * CODEX (阶段 8R Task 8R.10): wire these to the Settings panel UI. Keep the
 * shape stable — AppShell already applies `mode`, `accent`, and `density` to the
 * DOM via effects.
 */

export type AppMode = "recorder" | "player";
export type Density = "compact" | "regular" | "comfortable";
export type TimelineStyle = "compact" | "detail" | "wave";
export type RateUi = "buttons" | "slider" | "knob";
export type PluginLayout = "list" | "grid";
export type AccentColor = "#3a7bff" | "#5eead4" | "#b794f6" | "#ff5e7e";

export const UI_STORAGE_KEY = "recplay-ui";

export interface UiSettings {
  mode: AppMode;
  density: Density;
  accent: AccentColor;
  timelineStyle: TimelineStyle;
  rateUi: RateUi;
  pluginLayout: PluginLayout;
}

const DEFAULTS: UiSettings = {
  mode: "player",
  density: "regular",
  accent: "#3a7bff",
  timelineStyle: "detail",
  rateUi: "buttons",
  pluginLayout: "list",
};

interface UiStoreState extends UiSettings {
  setMode: (mode: AppMode) => void;
  setDensity: (density: Density) => void;
  setAccent: (accent: AccentColor) => void;
  setTimelineStyle: (timelineStyle: TimelineStyle) => void;
  setRateUi: (rateUi: RateUi) => void;
  setPluginLayout: (pluginLayout: PluginLayout) => void;
}

export function readInitialSettings(): UiSettings {
  if (typeof window === "undefined") {
    return DEFAULTS;
  }
  try {
    const raw = window.localStorage.getItem(UI_STORAGE_KEY);
    if (!raw) {
      return DEFAULTS;
    }
    const parsed = { ...DEFAULTS, ...(JSON.parse(raw) as Partial<UiSettings>) };
    return {
      ...parsed,
      mode: parsed.mode === "recorder" ? "recorder" : "player",
    };
  } catch {
    return DEFAULTS;
  }
}

function persist(settings: UiSettings): void {
  if (typeof window === "undefined") {
    return;
  }
  try {
    window.localStorage.setItem(UI_STORAGE_KEY, JSON.stringify(settings));
  } catch {
    /* storage unavailable — non-fatal */
  }
}

export const useUiStore = create<UiStoreState>((set, get) => {
  const save = () => {
    const { mode, density, accent, timelineStyle, rateUi, pluginLayout } = get();
    persist({ mode, density, accent, timelineStyle, rateUi, pluginLayout });
  };

  return {
    ...readInitialSettings(),
    setMode: (mode) => {
      set({ mode });
      save();
    },
    setDensity: (density) => {
      set({ density });
      save();
    },
    setAccent: (accent) => {
      set({ accent });
      save();
    },
    setTimelineStyle: (timelineStyle) => {
      set({ timelineStyle });
      save();
    },
    setRateUi: (rateUi) => {
      set({ rateUi });
      save();
    },
    setPluginLayout: (pluginLayout) => {
      set({ pluginLayout });
      save();
    },
  };
});
