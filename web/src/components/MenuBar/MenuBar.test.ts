import { describe, expect, it } from "vitest";

import {
  getConnectionMeta,
  getLocaleToggleMeta,
  getMenuSections,
  getThemeToggleMeta,
  toggleMenu,
  type MenuKey,
} from "./menuBarModel";

describe("toggleMenu", () => {
  it("opens a menu when none is active", () => {
    expect(toggleMenu(null, "file")).toBe("file");
  });

  it("closes the current menu when clicked again", () => {
    expect(toggleMenu("view", "view")).toBeNull();
  });

  it("switches to the newly selected menu", () => {
    expect(toggleMenu("file", "plugins")).toBe("plugins");
  });
});

describe("getConnectionMeta", () => {
  it("returns stable labels and tones for each websocket state", () => {
    const cases: Array<[MenuKey | null, "connecting" | "open" | "closed" | "error", string, string]> = [
      [null, "connecting", "Connecting", "warning"],
      [null, "open", "Live", "success"],
      [null, "closed", "Offline", "muted"],
      [null, "error", "Fault", "danger"],
    ];

    for (const [, status, label, tone] of cases) {
      expect(getConnectionMeta(status, "en-US")).toMatchObject({ label, tone });
    }
  });
});

describe("getThemeToggleMeta", () => {
  it("describes the opposite theme action for the current mode", () => {
    expect(getThemeToggleMeta("dark", "en-US")).toEqual({
      icon: "sun",
      label: "Light Theme",
      title: "Switch to light theme",
    });

    expect(getThemeToggleMeta("light", "en-US")).toEqual({
      icon: "moon",
      label: "Dark Theme",
      title: "Switch to dark theme",
    });
  });
});

describe("getLocaleToggleMeta", () => {
  it("describes the next locale toggle target", () => {
    expect(getLocaleToggleMeta("en-US")).toEqual({
      label: "中文",
      title: "Switch to Simplified Chinese",
      nextLocale: "zh-CN",
    });

    expect(getLocaleToggleMeta("zh-CN")).toEqual({
      label: "EN",
      title: "切换到英文",
      nextLocale: "en-US",
    });
  });
});

describe("getMenuSections", () => {
  it("returns localized menu labels and descriptions", () => {
    const englishFile = getMenuSections("en-US")[0]!;
    expect(englishFile.label).toBe("File");
    expect(englishFile.items[0]).toMatchObject({
      label: "Open Capture",
      description: "Load an rpcap or pcap session.",
    });

    const chineseFile = getMenuSections("zh-CN")[0]!;
    expect(chineseFile.label).toBe("文件");
    expect(chineseFile.items[0]).toMatchObject({
      label: "打开抓包",
      description: "加载 rpcap 或 pcap 会话。",
    });
  });
});
