export type WsTone = "success" | "warning" | "muted" | "danger";
export interface StorageEstimateLike {
  quota?: number;
  usage?: number;
}

export function getWsTone(status: "connecting" | "open" | "closed" | "error"): WsTone {
  switch (status) {
    case "open":
      return "success";
    case "connecting":
      return "warning";
    case "error":
      return "danger";
    case "closed":
    default:
      return "muted";
  }
}

export function getBufferUsagePercent(used: number, capacity: number): number {
  if (capacity <= 0) {
    return 0;
  }

  return Math.min(100, Math.max(0, Math.round((used / capacity) * 100)));
}

export function formatDiskQueue(bytes: number): string {
  return formatBytes(bytes);
}

export function formatClock(date: Date): string {
  const hours = String(date.getHours()).padStart(2, "0");
  const minutes = String(date.getMinutes()).padStart(2, "0");
  const seconds = String(date.getSeconds()).padStart(2, "0");
  return `${hours}:${minutes}:${seconds}`;
}

export function formatCpuUsage(value: number | null | undefined): string {
  if (typeof value !== "number" || !Number.isFinite(value) || value < 0) {
    return "N/A";
  }

  return `${clampPercent(value)}%`;
}

export function getStorageFreeBytes(estimate: StorageEstimateLike): number | null {
  if (
    typeof estimate.quota !== "number" ||
    typeof estimate.usage !== "number" ||
    !Number.isFinite(estimate.quota) ||
    !Number.isFinite(estimate.usage) ||
    estimate.quota <= 0
  ) {
    return null;
  }

  return Math.max(0, estimate.quota - estimate.usage);
}

export function formatStorageFree(bytes: number | null): string {
  if (bytes === null) {
    return "N/A";
  }

  return formatBytes(bytes);
}

function clampPercent(value: number): number {
  return Math.min(100, Math.max(0, Math.round(value)));
}

function formatBytes(bytes: number): string {
  if (bytes < 1024) {
    return `${bytes} B`;
  }
  if (bytes < 1024 * 1024) {
    return `${(bytes / 1024).toFixed(1)} KB`;
  }
  if (bytes < 1024 * 1024 * 1024) {
    return `${(bytes / (1024 * 1024)).toFixed(1)} MB`;
  }
  return `${(bytes / (1024 * 1024 * 1024)).toFixed(1)} GB`;
}
