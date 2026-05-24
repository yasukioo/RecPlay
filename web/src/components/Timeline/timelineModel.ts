import { getTranslations } from "../../i18n";
import type { Locale, StatsHistoryPoint, TimelineMarker } from "../../types";
import { calculateRelativeSeekTimestamp } from "../seekModel";

export const TIMELINE_FOCUS_ID = "transport-timeline";

export function createZoomWindow(totalPoints: number, zoomLevel: number): { start: number; end: number } {
  if (totalPoints <= 0) {
    return { start: 0, end: 0 };
  }

  const clampedZoom = Math.max(1, zoomLevel);
  const visibleCount = Math.max(1, Math.ceil(totalPoints / clampedZoom));
  return {
    start: Math.max(0, totalPoints - visibleCount),
    end: totalPoints,
  };
}

export function calculateTimelineSeekTimestamp(params: {
  clientX: number;
  left: number;
  width: number;
  durationNs: number;
}): number {
  return calculateRelativeSeekTimestamp(params);
}

export function buildDerivedMarkers(
  history: StatsHistoryPoint[],
  durationNs: number,
  locale: Locale = "en-US",
): TimelineMarker[] {
  if (history.length < 2 || durationNs <= 0) {
    return [];
  }

  const lastIndex = history.length - 1;
  const t = getTranslations(locale);

  return history.flatMap((point, index) => {
    if (index === 0) {
      return [];
    }

    const previous = history[index - 1]!;
    const deltaDrops = point.total_drops - previous.total_drops;
    if (deltaDrops <= 0) {
      return [];
    }

    const ratio = index / lastIndex;
    return [{
      timestamp_ns: Math.round(durationNs * ratio),
      label: t.timeline.dropMarker(deltaDrops),
      type: "error" as const,
    }];
  });
}

export function formatTimelineTimestamp(ns: number): string {
  const secondsTotal = Math.max(0, ns) / 1_000_000_000;
  const minutes = Math.floor(secondsTotal / 60);
  const seconds = (secondsTotal % 60).toFixed(1);
  return `${minutes}:${seconds.padStart(4, "0")}`;
}

export function getLoopRegionStyle(
  startNs: number | null,
  endNs: number | null,
  durationNs: number,
): { left: string; width: string } | null {
  if (startNs === null || endNs === null || durationNs <= 0 || endNs <= startNs) {
    return null;
  }

  const left = Math.min(100, Math.max(0, (startNs / durationNs) * 100));
  const right = Math.min(100, Math.max(0, (endNs / durationNs) * 100));

  if (right <= left) {
    return null;
  }

  return {
    left: `${left}%`,
    width: `${right - left}%`,
  };
}
