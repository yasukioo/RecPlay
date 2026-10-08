export function calculateRelativeSeekTimestamp(params: {
  clientX: number;
  left: number;
  width: number;
  durationNs: number;
}): number {
  const { clientX, left, width, durationNs } = params;
  if (width <= 0 || durationNs <= 0) {
    return 0;
  }

  const ratio = Math.min(1, Math.max(0, (clientX - left) / width));
  return Math.round(durationNs * ratio);
}

export function getPositionPercent(positionNs: number, durationNs: number): number {
  if (durationNs <= 0) {
    return 0;
  }

  return Math.min(100, Math.max(0, (positionNs / durationNs) * 100));
}

export function getDisplayedSeekPositionNs(
  livePositionNs: number,
  previewPositionNs: number | null,
  durationNs: number,
): number {
  const raw = previewPositionNs ?? livePositionNs;
  if (durationNs <= 0) {
    return Math.max(0, raw);
  }

  return Math.min(durationNs, Math.max(0, raw));
}
