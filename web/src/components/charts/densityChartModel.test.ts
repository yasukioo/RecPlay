import { describe, expect, it } from "vitest";

import {
  buildDensityAreaPath,
  buildDensityLinePath,
  densityMax,
  downsampleMax,
  formatAxisValue,
  formatBucketTime,
} from "./densityChartModel";

describe("densityChartModel", () => {
  it("returns values unchanged when at or below target", () => {
    expect(downsampleMax([1, 2, 3], 600)).toEqual([1, 2, 3]);
    expect(downsampleMax([], 600)).toEqual([]);
  });

  it("max-pools when downsampling above target", () => {
    const result = downsampleMax([1, 5, 2, 9, 3, 4], 3);
    expect(result).toEqual([5, 9, 4]);
  });

  it("computes max via loop", () => {
    expect(densityMax([3, 7, 2])).toBe(7);
    expect(densityMax([])).toBe(0);
  });

  it("builds a baseline path for empty input", () => {
    expect(buildDensityLinePath([], 800, 200)).toBe("M0 200");
    expect(buildDensityAreaPath([], 800, 200)).toBe("M0 200 L800 200 Z");
  });

  it("inverts y so the peak sits at the top of the box", () => {
    const path = buildDensityLinePath([0, 10], 100, 200, 10);
    // first point at baseline (y=200), second at top (y=0)
    expect(path).toBe("M0.0 200.0 L100.0 0.0");
  });

  it("closes the area path back to the baseline", () => {
    const area = buildDensityAreaPath([5], 100, 200, 10);
    expect(area.endsWith("L100.0 200 L0 200 Z")).toBe(true);
  });

  it("keeps a decimal for small fractional axis values, rounds larger ones", () => {
    expect(formatAxisValue(2.34)).toBe("2.3");
    expect(formatAxisValue(0)).toBe("0");
    expect(formatAxisValue(5)).toBe("5");
    expect(formatAxisValue(480)).toBe("480");
    expect(formatAxisValue(12.7)).toBe("13");
  });

  it("formats bucket time as mm:ss across the duration", () => {
    const oneMinuteNs = 60 * 1e9;
    expect(formatBucketTime(0, 5, oneMinuteNs)).toBe("00:00");
    expect(formatBucketTime(4, 5, oneMinuteNs)).toBe("01:00");
    expect(formatBucketTime(2, 5, oneMinuteNs)).toBe("00:30");
  });
});
