import { createElement } from "react";
import { renderToStaticMarkup } from "react-dom/server";
import { afterEach, describe, expect, it, vi } from "vitest";

import { RecordPanel } from "./RecordPanel";

const localeStoreState = vi.hoisted(() => ({
  locale: "en-US" as "en-US" | "zh-CN",
}));

const sessionStoreState = vi.hoisted(() => ({
  state: "Idle" as
    | "Idle"
    | "Recording"
    | "RecordingPaused"
    | "Playing"
    | "PlayingPaused"
    | "Unknown",
}));

vi.mock("../../stores/localeStore", () => ({
  useLocaleStore: (selector: (state: typeof localeStoreState) => unknown) => selector(localeStoreState),
}));

vi.mock("../../stores/sessionStore", () => ({
  useSessionStore: (
    selector: (state: {
      state: typeof sessionStoreState.state;
      duration_ns: number;
      position_ns: number;
      speed: number;
      loop_start_ns: null;
      loop_end_ns: null;
      config: { recordPath: string; playbackPath: string };
      lastTransition: null;
      replaceSnapshot: () => void;
      applyTransition: () => void;
      setLoopRegion: () => void;
      clearLoopRegion: () => void;
      updateConfig: () => void;
    }) => unknown,
  ) =>
    selector({
      state: sessionStoreState.state,
      duration_ns: 0,
      position_ns: 0,
      speed: 1,
      loop_start_ns: null,
      loop_end_ns: null,
      config: { recordPath: "", playbackPath: "" },
      lastTransition: null,
      replaceSnapshot: () => undefined,
      applyTransition: () => undefined,
      setLoopRegion: () => undefined,
      clearLoopRegion: () => undefined,
      updateConfig: () => undefined,
    }),
}));

afterEach(() => {
  localeStoreState.locale = "en-US";
  sessionStoreState.state = "Idle";
});

describe("RecordPanel", () => {
  it("renders the current session state prominently for recording workflows", () => {
    sessionStoreState.state = "Recording";

    const html = renderToStaticMarkup(createElement(RecordPanel, { onError: () => undefined }));

    expect(html).toContain('role="status"');
    expect(html).toContain("State");
    expect(html).toContain("Recording");
  });

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
