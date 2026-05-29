import { describe, expect, it } from "vitest";

import {
  getAppShellClasses,
  getMapperClasses,
  getMonitorDashboardClasses,
  getPanelGridClasses,
  getPluginCardClasses,
  getSessionInfoClasses,
  getStatusBarClasses,
} from "./layoutModel";

describe("getAppShellClasses", () => {
  it("keeps the footer bars visible by constraining the shell to the viewport and allowing the workspace to scroll", () => {
    expect(getAppShellClasses()).toEqual({
      app: "h-screen overflow-hidden bg-hmi-bg text-hmi-text",
      root: "flex h-full min-h-0 flex-col",
      main: "min-h-0 flex-1 overflow-y-auto xl:grid xl:grid-cols-[220px_minmax(0,1fr)_280px] xl:overflow-hidden",
      leftRail: "min-h-0 border-b border-hmi-border p-2 xl:overflow-y-auto xl:border-b-0 xl:border-r",
      center: "min-h-0 space-y-3 overflow-visible p-3 sm:p-4 xl:overflow-y-auto",
      rightRail: "min-h-0 border-t border-hmi-border p-3 sm:p-4 xl:overflow-y-auto xl:border-t-0 xl:border-l",
    });
  });
});

describe("getPanelGridClasses", () => {
  it("keeps stacked panels by default and upgrades to two columns at lg", () => {
    expect(getPanelGridClasses()).toBe("grid grid-cols-1 gap-3 lg:grid-cols-2");
  });
});

describe("getSessionInfoClasses", () => {
  it("uses shell-integrated spacing instead of a nested card on the right rail", () => {
    expect(getSessionInfoClasses()).toBe("space-y-4");
  });
});

describe("getStatusBarClasses", () => {
  it("wraps status content on narrow screens and returns to split alignment on md", () => {
    expect(getStatusBarClasses()).toEqual({
      footer: "flex flex-wrap items-center justify-between gap-x-4 gap-y-1 border-t border-hmi-border bg-hmi-surface px-3 py-2 text-xs sm:px-4",
      left: "flex flex-wrap items-center gap-x-4 gap-y-1",
      right: "flex w-full flex-wrap items-center gap-x-4 gap-y-1 text-hmi-text-muted md:w-auto md:justify-end",
    });
  });
});

describe("getMonitorDashboardClasses", () => {
  it("reduces dense grids to one or two columns before restoring the desktop density", () => {
    expect(getMonitorDashboardClasses()).toEqual({
      metricGrid: "grid grid-cols-1 gap-2 sm:grid-cols-2 xl:grid-cols-3",
      chartGrid: "grid grid-cols-1 gap-3 xl:grid-cols-[minmax(0,2fr)_minmax(220px,1fr)_minmax(220px,1fr)]",
      trendGrid: "grid grid-cols-1 gap-3 sm:grid-cols-2 xl:grid-cols-4",
    });
  });
});

describe("getMapperClasses", () => {
  it("drops the patch-bay center column on compact screens and stacks mapping editors", () => {
    expect(getMapperClasses()).toEqual({
      channelGrid: "grid grid-cols-1 gap-4 text-xs md:grid-cols-[minmax(0,1fr)_120px_minmax(0,1fr)]",
      mappingRow: "grid grid-cols-1 gap-2 rounded bg-hmi-surface-alt px-2 py-2 lg:grid-cols-[minmax(0,1fr)_minmax(0,1fr)_auto]",
    });
  });
});

describe("getPluginCardClasses", () => {
  it("stacks plugin config fields on small screens and restores two columns at md", () => {
    expect(getPluginCardClasses()).toEqual({
      configGrid: "grid grid-cols-1 gap-3 md:grid-cols-2",
      actions: "flex flex-wrap gap-2 border-t border-hmi-border pt-3",
    });
  });
});
