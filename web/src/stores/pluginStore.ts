import { create } from "zustand";
import type { PluginInfo } from "../types";

interface PluginStoreState {
  plugins: PluginInfo[];
  selectedPluginId: string | null;
  setPlugins: (plugins: PluginInfo[]) => void;
  updatePlugin: (id: string, patch: Partial<PluginInfo>) => void;
  togglePluginEnabled: (id: string) => void;
  movePlugin: (id: string, beforeId: string) => void;
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

  togglePluginEnabled: (id) =>
    set((state) => ({
      plugins: state.plugins.map((plugin) => {
        if (plugin.id !== id) {
          return plugin;
        }
        if (plugin.state === "active") {
          return { ...plugin, state: "inactive" };
        }
        if (plugin.state === "inactive") {
          return { ...plugin, state: "active" };
        }
        return plugin;
      }),
    })),

  movePlugin: (id, beforeId) =>
    set((state) => {
      const plugins = [...state.plugins];
      const fromIndex = plugins.findIndex((plugin) => plugin.id === id);
      const toIndex = plugins.findIndex((plugin) => plugin.id === beforeId);

      if (fromIndex === -1 || toIndex === -1 || fromIndex === toIndex) {
        return state;
      }

      const [plugin] = plugins.splice(fromIndex, 1);
      const nextIndex = toIndex;
      plugins.splice(nextIndex, 0, plugin!);
      return { plugins };
    }),

  selectPlugin: (id) => set({ selectedPluginId: id }),
}));
