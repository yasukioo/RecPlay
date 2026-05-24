import { createElement } from "react";
import { renderToStaticMarkup } from "react-dom/server";
import { afterEach, describe, expect, it, vi } from "vitest";

import { RecordPanel } from "./RecordPanel";

const localeStoreState = vi.hoisted(() => ({
  locale: "en-US" as "en-US" | "zh-CN",
}));

vi.mock("../../stores/localeStore", () => ({
  useLocaleStore: (selector: (state: typeof localeStoreState) => unknown) => selector(localeStoreState),
}));

afterEach(() => {
  localeStoreState.locale = "en-US";
});

describe("RecordPanel", () => {
  it("renders Simplified Chinese labels when the locale switches", () => {
    localeStoreState.locale = "zh-CN";

    const html = renderToStaticMarkup(createElement(RecordPanel, { onError: () => undefined }));

    expect(html).toContain("录制");
    expect(html).toContain("输出路径");
    expect(html).toContain("协议");
    expect(html).toContain("开始");
    expect(html).toContain("暂停");
    expect(html).toContain("继续");
    expect(html).toContain("停止");
  });
});
