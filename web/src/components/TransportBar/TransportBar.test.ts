import { describe, expect, it } from "vitest";

import * as transportModel from "./transportBarModel";
import {
  calculateSeekTimestamp,
  formatTransportTime,
  getProgressPercent,
  resolveTransportCommand,
} from "./transportBarModel";

describe("resolveTransportCommand", () => {
  it("maps recording controls based on session state", () => {
    expect(resolveTransportCommand("record", "Idle")).toBe("startRecording");
    expect(resolveTransportCommand("record", "Recording")).toBe("pauseRecording");
    expect(resolveTransportCommand("record", "RecordingPaused")).toBe("resumeRecording");
    expect(resolveTransportCommand("stop", "Recording")).toBe("stopRecording");
    expect(resolveTransportCommand("stop", "RecordingPaused")).toBe("stopRecording");
  });

  it("maps playback controls based on session state", () => {
    expect(resolveTransportCommand("play", "Idle")).toBe("playPlayback");
    expect(resolveTransportCommand("play", "Stopped" as never)).toBe("playPlayback");
    expect(resolveTransportCommand("play", "PlaybackPaused" as never)).toBe("playPlayback");
    expect(resolveTransportCommand("pause", "Playing")).toBe("pausePlayback");
    expect(resolveTransportCommand("stop", "Playing")).toBe("stopPlayback");
    expect(resolveTransportCommand("stop", "PlaybackPaused" as never)).toBe("stopPlayback");
  });

  it("returns null for unsupported state and control combinations", () => {
    expect(resolveTransportCommand("pause", "Idle")).toBeNull();
    expect(resolveTransportCommand("play", "Recording")).toBeNull();
    expect(resolveTransportCommand("record", "Playing")).toBeNull();
  });
});

describe("formatTransportTime", () => {
  it("renders ns values as mm:ss.s", () => {
    expect(formatTransportTime(0)).toBe("0:00.0");
    expect(formatTransportTime(65_400_000_000)).toBe("1:05.4");
  });
});

describe("getProgressPercent", () => {
  it("clamps progress between zero and one hundred", () => {
    expect(getProgressPercent(50, 200)).toBe(25);
    expect(getProgressPercent(250, 200)).toBe(100);
    expect(getProgressPercent(50, 0)).toBe(0);
  });
});

describe("calculateSeekTimestamp", () => {
  it("converts pointer position into a duration-relative timestamp", () => {
    expect(calculateSeekTimestamp({ clientX: 150, left: 100, width: 200, durationNs: 1_000 })).toBe(250);
    expect(calculateSeekTimestamp({ clientX: 350, left: 100, width: 200, durationNs: 1_000 })).toBe(1_000);
    expect(calculateSeekTimestamp({ clientX: 50, left: 100, width: 200, durationNs: 1_000 })).toBe(0);
  });
});

describe("resolveGlobalShortcut", () => {
  it("maps transport shortcuts for playback and record controls", () => {
    expect(
      transportModel.resolveGlobalShortcut({
        code: "Space",
        key: " ",
        altKey: false,
        ctrlKey: false,
        metaKey: false,
        shiftKey: false,
      }, "Idle"),
    ).toEqual({ type: "transport", control: "play" });

    expect(
      transportModel.resolveGlobalShortcut({
        code: "Space",
        key: " ",
        altKey: false,
        ctrlKey: false,
        metaKey: false,
        shiftKey: false,
      }, "Playing"),
    ).toEqual({ type: "transport", control: "pause" });

    expect(
      transportModel.resolveGlobalShortcut({
        code: "Space",
        key: " ",
        altKey: false,
        ctrlKey: false,
        metaKey: false,
        shiftKey: false,
      }, "PlaybackPaused" as never),
    ).toEqual({ type: "transport", control: "play" });

    expect(
      transportModel.resolveGlobalShortcut({
        code: "KeyR",
        key: "r",
        altKey: false,
        ctrlKey: false,
        metaKey: false,
        shiftKey: false,
      }, "RecordingPaused"),
    ).toEqual({ type: "transport", control: "record" });

    expect(
      transportModel.resolveGlobalShortcut({
        code: "KeyS",
        key: "s",
        altKey: false,
        ctrlKey: false,
        metaKey: false,
        shiftKey: false,
      }, "PlaybackPaused" as never),
    ).toEqual({ type: "transport", control: "stop" });
  });

  it("maps Alt+1 to timeline focus without stealing browser-style modifiers", () => {
    expect(
      transportModel.resolveGlobalShortcut({
        code: "Digit1",
        key: "1",
        altKey: true,
        ctrlKey: false,
        metaKey: false,
        shiftKey: false,
      }, "Idle"),
    ).toEqual({ type: "focusTimeline" });

    expect(
      transportModel.resolveGlobalShortcut({
        code: "KeyR",
        key: "r",
        altKey: false,
        ctrlKey: true,
        metaKey: false,
        shiftKey: false,
      }, "Idle"),
    ).toBeNull();
  });

  it("ignores shortcuts while typing in editable fields", () => {
    expect(
      transportModel.resolveGlobalShortcut({
        code: "Space",
        key: " ",
        altKey: false,
        ctrlKey: false,
        metaKey: false,
        shiftKey: false,
        targetTagName: "input",
        targetIsContentEditable: false,
      }, "Idle"),
    ).toBeNull();

    expect(
      transportModel.resolveGlobalShortcut({
        code: "KeyS",
        key: "s",
        altKey: false,
        ctrlKey: false,
        metaKey: false,
        shiftKey: false,
        targetTagName: "div",
        targetIsContentEditable: true,
      }, "Playing"),
    ).toBeNull();
  });
});
