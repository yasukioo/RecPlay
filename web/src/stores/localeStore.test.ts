import { afterEach, describe, expect, it } from "vitest";

import { getInitialLocale, sanitizeLocale, useLocaleStore } from "./localeStore";

afterEach(() => {
  useLocaleStore.setState({ locale: "en-US" });
});

describe("sanitizeLocale", () => {
  it("accepts supported locales and falls back to English", () => {
    expect(sanitizeLocale("zh-CN")).toBe("zh-CN");
    expect(sanitizeLocale("en-US")).toBe("en-US");
    expect(sanitizeLocale("ja-JP")).toBe("en-US");
    expect(sanitizeLocale(null)).toBe("en-US");
  });

  it("reads the initial locale from storage", () => {
    expect(getInitialLocale({ getItem: () => "zh-CN" })).toBe("zh-CN");
    expect(getInitialLocale({ getItem: () => "ja-JP" })).toBe("en-US");
    expect(getInitialLocale(null)).toBe("en-US");
  });
});

describe("localeStore", () => {
  it("toggles between English and Simplified Chinese", () => {
    expect(useLocaleStore.getState().locale).toBe("en-US");

    useLocaleStore.getState().toggleLocale();
    expect(useLocaleStore.getState().locale).toBe("zh-CN");

    useLocaleStore.getState().toggleLocale();
    expect(useLocaleStore.getState().locale).toBe("en-US");
  });

  it("supports explicitly setting the active locale", () => {
    useLocaleStore.getState().setLocale("zh-CN");
    expect(useLocaleStore.getState().locale).toBe("zh-CN");

    useLocaleStore.getState().setLocale("en-US");
    expect(useLocaleStore.getState().locale).toBe("en-US");
  });
});
