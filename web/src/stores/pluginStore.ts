import { create } from "zustand";
import type { PluginInfo } from "../types";

interface PluginStoreState {
  plugins: PluginInfo[];
  selectedPluginId: string | null;
  setPlugins: (plugins: PluginInfo[]) => void;
  updatePlugin: (id: string, patch: Partial<PluginInfo>) => void;
  selectPlugin: (id: string | null) => void;
}

export const usePluginStore = create<PluginStoreState>((set) => ({
  plugins: [],
  selectedPluginId: null,

  setPlugins: (plugins) => set({ plugins }),

  updatePlugin: (id, patch) =>
    set((state) => ({
      plugins: state.plugins.map((p) => (p.id === id ? { ...p, ...patch } : p)),
    })),

  selectPlugin: (id) => set({ selectedPluginId: id }),
}));
