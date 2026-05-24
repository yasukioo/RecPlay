import { describe, expect, it } from "vitest";

import {
  calculateRelativeSeekTimestamp,
  getDisplayedSeekPositionNs,
  getPositionPercent,
} from "./seekModel";

describe("calculateRelativeSeekTimestamp", () => {
  it("maps pointer positions to clamped timestamps", () => {
    expect(calculateRelativeSeekTimestamp({ clientX: 120, left: 100, width: 200, durationNs: 1_000 })).toBe(100);
    expect(calculateRelativeSeekTimestamp({ clientX: 20, left: 100, width: 200, durationNs: 1_000 })).toBe(0);
    expect(calculateRelativeSeekTimestamp({ clientX: 320, left: 100, width: 200, durationNs: 1_000 })).toBe(1_000);
  });
});

describe("getPositionPercent", () => {
  it("clamps percentages between zero and one hundred", () => {
    expect(getPositionPercent(500, 1_000)).toBe(50);
    expect(getPositionPercent(2_000, 1_000)).toBe(100);
    expect(getPositionPercent(100, 0)).toBe(0);
  });
});

describe("getDisplayedSeekPositionNs", () => {
  it("prefers the drag preview position when present", () => {
    expect(getDisplayedSeekPositionNs(200, 800, 1_000)).toBe(800);
  });

  it("falls back to the live position when there is no preview", () => {
    expect(getDisplayedSeekPositionNs(200, null, 1_000)).toBe(200);
  });

  it("clamps preview values into the valid duration range", () => {
    expect(getDisplayedSeekPositionNs(200, -50, 1_000)).toBe(0);
    expect(getDisplayedSeekPositionNs(200, 5_000, 1_000)).toBe(1_000);
  });
});
