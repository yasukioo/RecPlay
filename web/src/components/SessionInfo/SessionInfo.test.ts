import { createElement } from "react";
import { renderToStaticMarkup } from "react-dom/server";
import { afterEach, describe, expect, it, vi } from "vitest";

import { SessionInfo } from "./SessionInfo";

const localeState = vi.hoisted(() => ({
  locale: "en-US" as "en-US" | "zh-CN",
}));

const sessionState = vi.hoisted(() => ({
  state: "Idle" as
    | "Idle"
    | "Recording"
    | "RecordingPaused"
    | "Playing"
    | "PlayingPaused"
    | "Unknown",
  duration_ns: 0,
  position_ns: 0,
  speed: 1,
  loop_start_ns: null as number | null,
  loop_end_ns: null as number | null,
  config: { recordPath: "", playbackPath: "" },
  lastTransition: null as null | { old_state: string; new_state: string },
}));

vi.mock("../../api/client", () => ({
  listRecordingFiles: vi.fn(() => Promise.resolve({ files: ["capture-a.rpcp"] })),
}));

vi.mock("../../stores/localeStore", () => ({
  useLocaleStore: (
    selector: (state: {
      locale: typeof localeState.locale;
      setLocale: () => void;
      toggleLocale: () => void;
    }) => unknown,
  ) =>
    selector({
      locale: localeState.locale,
      setLocale: () => undefined,
      toggleLocale: () => undefined,
    }),
}));

vi.mock("../../stores/sessionStore", () => ({
  useSessionStore: (
    selector: (state: typeof sessionState & {
      replaceSnapshot: () => void;
      applyTransition: () => void;
      setLoopRegion: () => void;
      clearLoopRegion: () => void;
      updateConfig: () => void;
    }) => unknown,
  ) =>
    selector({
      ...sessionState,
      replaceSnapshot: () => undefined,
      applyTransition: () => undefined,
      setLoopRegion: () => undefined,
      clearLoopRegion: () => undefined,
      updateConfig: () => undefined,
    }),
}));

afterEach(() => {
  localeState.locale = "en-US";
  sessionState.state = "Idle";
  sessionState.duration_ns = 0;
  sessionState.position_ns = 0;
  sessionState.speed = 1;
  sessionState.loop_start_ns = null;
  sessionState.loop_end_ns = null;
  sessionState.config = { recordPath: "", playbackPath: "" };
  sessionState.lastTransition = null;
});

describe("SessionInfo", () => {
  it("renders localized playback details and transition labels", () => {
    localeState.locale = "zh-CN";
    sessionState.state = "Playing";
    sessionState.duration_ns = 125_000_000_000;
    sessionState.position_ns = 5_000_000_000;
    sessionState.speed = 1.5;
    sessionState.loop_start_ns = 10_000_000_000;
    sessionState.loop_end_ns = 20_000_000_000;
    sessionState.config = { recordPath: "", playbackPath: "captures/demo.rpcp" };
    sessionState.lastTransition = { old_state: "Idle", new_state: "Playing" };

    const html = renderToStaticMarkup(createElement(SessionInfo));

    expect(html).toContain("会话");
    expect(html).toContain("状态");
    expect(html).toContain("位置");
    expect(html).toContain("时长");
    expect(html).toContain("回放配置");
    expect(html).toContain("循环区间");
    expect(html).toContain("最近一次");
    expect(html).toContain("回放中");
    expect(html).toContain("空闲");
    expect(html).not.toContain("Playing");
    expect(html).toContain("captures/demo.rpcp");
    expect(html).toContain("10.00s");
    expect(html).toContain("20.00s");
    expect(html).toContain("1.50x");
  });

  it("renders recording output details for recording sessions", () => {
    localeState.locale = "zh-CN";
    sessionState.state = "RecordingPaused";
    sessionState.config = { recordPath: "captures/live.rpcp", playbackPath: "" };

    const html = renderToStaticMarkup(createElement(SessionInfo));

    expect(html).toContain("录制配置");
    expect(html).toContain("输出");
    expect(html).toContain("captures/live.rpcp");
    expect(html).toContain("录制已暂停");
  });
});
