import { afterEach, describe, expect, it, vi } from "vitest";

afterEach(() => {
  vi.unstubAllGlobals();
  vi.resetModules();
});

describe("themeStore initialization", () => {
  it("reads the initial theme from storage", async () => {
    vi.stubGlobal("window", {
      localStorage: {
        getItem: vi.fn().mockReturnValue("light"),
      },
    });

    const { useThemeStore } = await import("./themeStore");

    expect(useThemeStore.getState().theme).toBe("light");
  });

  it("falls back to dark when the saved theme is unsupported", async () => {
    vi.stubGlobal("window", {
      localStorage: {
        getItem: vi.fn().mockReturnValue("system"),
      },
    });

    const { useThemeStore } = await import("./themeStore");

    expect(useThemeStore.getState().theme).toBe("dark");
  });
});
