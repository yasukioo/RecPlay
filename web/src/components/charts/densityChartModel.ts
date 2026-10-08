/**
 * Pure helpers for the Event Density chart (阶段 8R Task 8R.7).
 *
 * Renders a single total-density series (packets per time bucket) over the full
 * file duration. Ported from the design稿 viz.jsx ThroughputChart approach:
 * downsample for performance, then build SVG area + line paths in a w×h box.
 * Multi-channel (per-channel bands) is a future extension — keep the API shaped
 * so a `series[]` variant can layer on top.
 */

/** Max-pool downsample so a long bucket array stays cheap to render (~target points). */
export function downsampleMax(values: number[], target: number): number[] {
  if (target <= 0 || values.length <= target) {
    return values.slice();
  }
  const step = Math.ceil(values.length / target);
  const out: number[] = [];
  for (let i = 0; i < values.length; i += step) {
    let peak = 0;
    for (let k = 0; k < step && i + k < values.length; k += 1) {
      if (values[i + k] > peak) {
        peak = values[i + k];
      }
    }
    out.push(peak);
  }
  return out;
}

/** Largest value (loop, not spread, to stay safe on large arrays). */
export function densityMax(values: number[]): number {
  let max = 0;
  for (const value of values) {
    if (value > max) {
      max = value;
    }
  }
  return max;
}

/** SVG polyline path mapping values into a [0,width]×[0,height] box (y inverted). */
export function buildDensityLinePath(
  values: number[],
  width: number,
  height: number,
  maxValue?: number,
): string {
  if (values.length === 0) {
    return `M0 ${height}`;
  }
  const max = Math.max(1, maxValue ?? densityMax(values));
  const n = values.length;
  const x = (i: number) => (n === 1 ? 0 : (i / (n - 1)) * width);
  const y = (v: number) => height - (v / max) * height;
  let path = `M${x(0).toFixed(1)} ${y(values[0]).toFixed(1)}`;
  for (let i = 1; i < n; i += 1) {
    path += ` L${x(i).toFixed(1)} ${y(values[i]).toFixed(1)}`;
  }
  return path;
}

/** Closed area path under the line (down to the baseline). */
export function buildDensityAreaPath(
  values: number[],
  width: number,
  height: number,
  maxValue?: number,
): string {
  if (values.length === 0) {
    return `M0 ${height} L${width} ${height} Z`;
  }
  return `${buildDensityLinePath(values, width, height, maxValue)} L${width.toFixed(1)} ${height} L0 ${height} Z`;
}

/** Y-axis tick label: keep one decimal for small fractional magnitudes (e.g.
 * sub-10 Mbps throughput), otherwise round to an integer (e.g. packet counts). */
export function formatAxisValue(value: number): string {
  if (!Number.isFinite(value)) {
    return "0";
  }
  const abs = Math.abs(value);
  if (abs !== 0 && abs < 10 && !Number.isInteger(value)) {
    return value.toFixed(1);
  }
  return String(Math.round(value));
}

/** mm:ss label for the time at fraction `index/(count-1)` of the duration. */
export function formatBucketTime(index: number, count: number, durationNs: number): string {
  const fraction = count <= 1 ? 0 : index / (count - 1);
  const totalSeconds = Math.max(0, Math.floor((durationNs / 1e9) * fraction));
  const minutes = Math.floor(totalSeconds / 60);
  const seconds = totalSeconds % 60;
  return `${String(minutes).padStart(2, "0")}:${String(seconds).padStart(2, "0")}`;
}
