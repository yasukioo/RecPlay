import { createElement } from "react";
import { renderToStaticMarkup } from "react-dom/server";
import { afterEach, describe, expect, it, vi } from "vitest";

import { PlaybackPanel } from "./PlaybackPanel";

const localeStoreState = vi.hoisted(() => ({
  locale: "en-US" as "en-US" | "zh-CN",
}));

vi.mock("../../stores/localeStore", () => ({
  useLocaleStore: (selector: (state: typeof localeStoreState) => unknown) => selector(localeStoreState),
}));

afterEach(() => {
  localeStoreState.locale = "en-US";
});

describe("PlaybackPanel", () => {
  it("renders Simplified Chinese labels when the locale switches", () => {
    localeStoreState.locale = "zh-CN";

    const html = renderToStaticMarkup(createElement(PlaybackPanel, { onError: () => undefined }));

    expect(html).toContain("回放");
    expect(html).toContain("文件路径");
    expect(html).toContain("速度");
    expect(html).toContain("定位 (ns)");
    expect(html).toContain("循环开始");
    expect(html).toContain("循环结束");
    expect(html).toContain("打开");
    expect(html).toContain("播放");
    expect(html).toContain("暂停");
    expect(html).toContain("循环");
    expect(html).toContain("停止");
  });
});
