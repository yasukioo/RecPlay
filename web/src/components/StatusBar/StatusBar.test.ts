import { describe, expect, it } from "vitest";

import {
  formatCpuUsage,
  formatClock,
  formatDiskQueue,
  formatStorageFree,
  getBufferUsagePercent,
  getStorageFreeBytes,
  getWsTone,
} from "./statusBarModel";

describe("getWsTone", () => {
  it("maps websocket states to stable tones", () => {
    expect(getWsTone("open")).toBe("success");
    expect(getWsTone("connecting")).toBe("warning");
    expect(getWsTone("closed")).toBe("muted");
    expect(getWsTone("error")).toBe("danger");
  });
});

describe("getBufferUsagePercent", () => {
  it("returns a clamped percentage", () => {
    expect(getBufferUsagePercent(50, 200)).toBe(25);
    expect(getBufferUsagePercent(500, 200)).toBe(100);
    expect(getBufferUsagePercent(50, 0)).toBe(0);
  });
});

describe("formatDiskQueue", () => {
  it("formats byte counts with units", () => {
    expect(formatDiskQueue(512)).toBe("512 B");
    expect(formatDiskQueue(2_048)).toBe("2.0 KB");
    expect(formatDiskQueue(5_242_880)).toBe("5.0 MB");
  });
});

describe("formatClock", () => {
  it("renders a zero-padded hh:mm:ss clock", () => {
    const date = new Date("2026-05-18T03:04:05");
    expect(formatClock(date)).toBe("03:04:05");
  });
});

describe("formatCpuUsage", () => {
  it("renders actual cpu usage percentages and gracefully handles unavailable data", () => {
    expect(formatCpuUsage(37.5)).toBe("38%");
    expect(formatCpuUsage(0)).toBe("0%");
    expect(formatCpuUsage(null)).toBe("N/A");
    expect(formatCpuUsage(-1)).toBe("N/A");
  });
});

describe("getStorageFreeBytes", () => {
  it("calculates free space from a browser storage estimate", () => {
    expect(getStorageFreeBytes({ quota: 10_000, usage: 4_000 })).toBe(6_000);
    expect(getStorageFreeBytes({ quota: 10_000, usage: 12_000 })).toBe(0);
    expect(getStorageFreeBytes({})).toBeNull();
  });
});

describe("formatStorageFree", () => {
  it("formats free space and handles unavailable estimates", () => {
    expect(formatStorageFree(null)).toBe("N/A");
    expect(formatStorageFree(512)).toBe("512 B");
    expect(formatStorageFree(5 * 1024 * 1024)).toBe("5.0 MB");
    expect(formatStorageFree(3 * 1024 * 1024 * 1024)).toBe("3.0 GB");
  });
});
