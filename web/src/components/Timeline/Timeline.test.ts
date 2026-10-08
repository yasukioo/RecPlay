import { describe, expect, it } from "vitest";

import {
  buildDerivedMarkers,
  calculateTimelineSeekTimestamp,
  createZoomWindow,
  formatTimelineTimestamp,
  getLoopRegionStyle,
} from "./timelineModel";

describe("createZoomWindow", () => {
  it("returns the full history when zoom is one", () => {
    expect(createZoomWindow(10, 1)).toEqual({ start: 0, end: 10 });
  });

  it("keeps the newest samples when zooming in", () => {
    expect(createZoomWindow(20, 4)).toEqual({ start: 15, end: 20 });
  });
});

describe("calculateTimelineSeekTimestamp", () => {
  it("maps x coordinates to timeline timestamps", () => {
    expect(calculateTimelineSeekTimestamp({ clientX: 100, left: 100, width: 200, durationNs: 1_000 })).toBe(0);
    expect(calculateTimelineSeekTimestamp({ clientX: 200, left: 100, width: 200, durationNs: 1_000 })).toBe(500);
    expect(calculateTimelineSeekTimestamp({ clientX: 400, left: 100, width: 200, durationNs: 1_000 })).toBe(1_000);
  });
});

describe("buildDerivedMarkers", () => {
  it("creates error markers when drop counts increase", () => {
    const markers = buildDerivedMarkers([
      { timestamp: 1, total_packets: 10, total_drops: 0, total_throughput_mbps: 1 },
      { timestamp: 2, total_packets: 20, total_drops: 3, total_throughput_mbps: 1.2 },
      { timestamp: 3, total_packets: 30, total_drops: 3, total_throughput_mbps: 1.1 },
      { timestamp: 4, total_packets: 40, total_drops: 5, total_throughput_mbps: 1.3 },
    ], 1_000, "en-US");

    expect(markers).toEqual([
      { timestamp_ns: 333, label: "+3 drops", type: "error" },
      { timestamp_ns: 1_000, label: "+2 drops", type: "error" },
    ]);
  });

  it("localizes derived drop markers", () => {
    const markers = buildDerivedMarkers([
      { timestamp: 1, total_packets: 10, total_drops: 0, total_throughput_mbps: 1 },
      { timestamp: 2, total_packets: 20, total_drops: 2, total_throughput_mbps: 1.2 },
    ], 1_000, "zh-CN");

    expect(markers[0]).toMatchObject({ label: "+2 次丢包" });
  });
});

describe("formatTimelineTimestamp", () => {
  it("formats ns timestamps as mm:ss.s", () => {
    expect(formatTimelineTimestamp(65_400_000_000)).toBe("1:05.4");
  });
});

describe("getLoopRegionStyle", () => {
  it("converts a loop range into timeline overlay percentages", () => {
    expect(getLoopRegionStyle(200, 600, 1_000)).toEqual({
      left: "20%",
      width: "40%",
    });
  });

  it("returns null for invalid or empty loop ranges", () => {
    expect(getLoopRegionStyle(null, 600, 1_000)).toBeNull();
    expect(getLoopRegionStyle(700, 600, 1_000)).toBeNull();
    expect(getLoopRegionStyle(200, 600, 0)).toBeNull();
  });
});
