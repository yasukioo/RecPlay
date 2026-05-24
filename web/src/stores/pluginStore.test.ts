import { afterEach, describe, expect, it } from "vitest";

import { usePluginStore } from "./pluginStore";

const plugins = [
  { id: "alpha", name: "Alpha", version: "1.0.0", state: "active", bundle_path: "a" },
  { id: "beta", name: "Beta", version: "1.0.0", state: "inactive", bundle_path: "b" },
  { id: "gamma", name: "Gamma", version: "1.0.0", state: "error", bundle_path: "c" },
] as const;

afterEach(() => {
  usePluginStore.setState({ plugins: [], selectedPluginId: null });
});

describe("pluginStore", () => {
  it("toggles active and inactive plugins", () => {
    usePluginStore.getState().setPlugins([...plugins]);

    usePluginStore.getState().togglePluginEnabled("alpha");
    usePluginStore.getState().togglePluginEnabled("beta");
    usePluginStore.getState().togglePluginEnabled("gamma");

    expect(usePluginStore.getState().plugins.map((plugin) => plugin.state)).toEqual([
      "inactive",
      "active",
      "error",
    ]);
  });

  it("moves a plugin before a target plugin", () => {
    usePluginStore.getState().setPlugins([...plugins]);

    usePluginStore.getState().movePlugin("gamma", "alpha");

    expect(usePluginStore.getState().plugins.map((plugin) => plugin.id)).toEqual([
      "gamma",
      "alpha",
      "beta",
    ]);
  });

  it("moves a plugin down when the target plugin is the next sibling", () => {
    usePluginStore.getState().setPlugins([...plugins]);

    usePluginStore.getState().movePlugin("alpha", "beta");

    expect(usePluginStore.getState().plugins.map((plugin) => plugin.id)).toEqual([
      "beta",
      "alpha",
      "gamma",
    ]);
  });
});
