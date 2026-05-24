import { afterEach, describe, expect, it } from "vitest";

import { sanitizeTheme, useThemeStore } from "./themeStore";

afterEach(() => {
  useThemeStore.setState({ theme: "dark" });
});

describe("sanitizeTheme", () => {
  it("accepts the supported modes and falls back to dark", () => {
    expect(sanitizeTheme("light")).toBe("light");
    expect(sanitizeTheme("dark")).toBe("dark");
    expect(sanitizeTheme("system")).toBe("dark");
    expect(sanitizeTheme(null)).toBe("dark");
  });
});

describe("themeStore", () => {
  it("toggles between dark and light themes", () => {
    expect(useThemeStore.getState().theme).toBe("dark");

    useThemeStore.getState().toggleTheme();
    expect(useThemeStore.getState().theme).toBe("light");

    useThemeStore.getState().toggleTheme();
    expect(useThemeStore.getState().theme).toBe("dark");
  });

  it("supports explicitly setting the active theme", () => {
    useThemeStore.getState().setTheme("light");
    expect(useThemeStore.getState().theme).toBe("light");

    useThemeStore.getState().setTheme("dark");
    expect(useThemeStore.getState().theme).toBe("dark");
  });
});
