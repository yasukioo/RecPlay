import type { SessionState } from "../../types";
import {
  calculateRelativeSeekTimestamp,
  getPositionPercent,
} from "../seekModel";

export type TransportControl = "record" | "play" | "pause" | "stop";

export type TransportCommand =
  | "startRecording"
  | "pauseRecording"
  | "resumeRecording"
  | "playPlayback"
  | "pausePlayback"
  | "stopRecording"
  | "stopPlayback";

export interface GlobalShortcutInput {
  key: string;
  code: string;
  altKey: boolean;
  ctrlKey: boolean;
  metaKey: boolean;
  shiftKey: boolean;
  repeat?: boolean;
  targetTagName?: string;
  targetIsContentEditable?: boolean;
}

export type GlobalShortcutAction =
  | { type: "transport"; control: TransportControl }
  | { type: "focusTimeline" };

export function formatTransportTime(ns: number): string {
  const sec = Math.max(0, ns) / 1_000_000_000;
  const minutes = Math.floor(sec / 60);
  const seconds = (sec % 60).toFixed(1);
  return `${minutes}:${seconds.padStart(4, "0")}`;
}

export function getProgressPercent(positionNs: number, durationNs: number): number {
  return getPositionPercent(positionNs, durationNs);
}

export function resolveTransportCommand(
  control: TransportControl,
  state: SessionState,
): TransportCommand | null {
  switch (control) {
    case "record":
      if (state === "Idle" || state === "Unknown") {
        return "startRecording";
      }
      if (state === "Recording") {
        return "pauseRecording";
      }
      if (state === "RecordingPaused") {
        return "resumeRecording";
      }
      return null;
    case "play":
      if (
        state === "Idle" ||
        state === "Unknown" ||
        state === "Stopped" ||
        state === "PlaybackPaused" ||
        state === "PlayingPaused"
      ) {
        return "playPlayback";
      }
      return null;
    case "pause":
      if (state === "Recording") {
        return "pauseRecording";
      }
      if (state === "Playing") {
        return "pausePlayback";
      }
      return null;
    case "stop":
      if (state === "Recording" || state === "RecordingPaused") {
        return "stopRecording";
      }
      if (
        state === "Playing" ||
        state === "PlaybackPaused" ||
        state === "PlayingPaused" ||
        state === "Seeking"
      ) {
        return "stopPlayback";
      }
      return null;
    default:
      return null;
  }
}

export function resolveGlobalShortcut(
  input: GlobalShortcutInput,
  state: SessionState,
): GlobalShortcutAction | null {
  if (input.repeat || isEditableShortcutTarget(input.targetTagName, input.targetIsContentEditable)) {
    return null;
  }

  const key = input.key.toLowerCase();

  if (input.altKey && !input.ctrlKey && !input.metaKey && !input.shiftKey && (input.code === "Digit1" || key === "1")) {
    return { type: "focusTimeline" };
  }

  if (input.ctrlKey || input.metaKey || input.altKey) {
    return null;
  }

  if (input.code === "Space" || input.key === " ") {
    const control: TransportControl = state === "Playing" ? "pause" : "play";
    return resolveTransportCommand(control, state) ? { type: "transport", control } : null;
  }

  if (input.code === "KeyR" || key === "r") {
    return resolveTransportCommand("record", state) ? { type: "transport", control: "record" } : null;
  }

  if (input.code === "KeyS" || key === "s") {
    return resolveTransportCommand("stop", state) ? { type: "transport", control: "stop" } : null;
  }

  return null;
}

export function calculateSeekTimestamp(params: {
  clientX: number;
  left: number;
  width: number;
  durationNs: number;
}): number {
  return calculateRelativeSeekTimestamp(params);
}

function isEditableShortcutTarget(tagName?: string, isContentEditable?: boolean): boolean {
  if (isContentEditable) {
    return true;
  }

  const normalizedTag = tagName?.toLowerCase();
  return normalizedTag === "input" || normalizedTag === "textarea" || normalizedTag === "select";
}
