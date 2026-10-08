import { describe, expect, it } from "vitest";

import { getPluginMenuActions } from "./pluginTreeModel";

describe("getPluginMenuActions", () => {
  it("returns state-aware actions for active and inactive plugins", () => {
    expect(getPluginMenuActions("active")).toEqual([
      { key: "select", label: "Select" },
      { key: "toggle", label: "Disable" },
    ]);

    expect(getPluginMenuActions("inactive")).toEqual([
      { key: "select", label: "Select" },
      { key: "toggle", label: "Enable" },
    ]);
  });

  it("omits toggle for error plugins", () => {
    expect(getPluginMenuActions("error")).toEqual([
      { key: "select", label: "Select" },
    ]);
  });

  it("localizes context menu labels", () => {
    expect(getPluginMenuActions("inactive", "zh-CN")).toEqual([
      { key: "select", label: "选择" },
      { key: "toggle", label: "启用" },
    ]);
  });
});
