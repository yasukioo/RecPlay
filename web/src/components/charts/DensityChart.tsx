import { useMemo } from "react";

import {
  buildDensityAreaPath,
  buildDensityLinePath,
  densityMax,
  downsampleMax,
  formatAxisValue,
  formatBucketTime,
} from "./densityChartModel";

interface DensityChartProps {
  /** Packet counts per time bucket across the full file duration. */
  values: number[];
  /** Duration of the visible window in ns (used for x-axis scale). */
  durationNs: number;
  /**
   * Start of the visible window in ns, relative to recording/file origin.
   * When set, x-axis labels show absolute elapsed time instead of 0-based.
   * Defaults to 0 (whole-file or short-window view).
   */
  offsetNs?: number;
}

const VIEW_W = 800;
const VIEW_H = 220;
const PAD = { left: 40, right: 12, top: 14, bottom: 22 };
const PLOT_W = VIEW_W - PAD.left - PAD.right;
const PLOT_H = VIEW_H - PAD.top - PAD.bottom;
const GRID = [0, 0.25, 0.5, 0.75, 1];
const TARGET_POINTS = 600;

/**
 * Event Density chart — total packets per time bucket over the whole recording.
 * Hand-rolled SVG (matches the design稿), colors driven by CSS variables so it
 * follows the active theme/accent. Data comes from GET /api/playback/density.
 */
export function DensityChart({ values, durationNs, offsetNs = 0 }: DensityChartProps) {
  const series = useMemo(() => downsampleMax(values ?? [], TARGET_POINTS), [values]);
  const max = useMemo(() => Math.max(1, densityMax(series)), [series]);
  const areaPath = useMemo(() => buildDensityAreaPath(series, PLOT_W, PLOT_H, max), [series, max]);
  const linePath = useMemo(() => buildDensityLinePath(series, PLOT_W, PLOT_H, max), [series, max]);

  return (
    <svg
      viewBox={`0 0 ${VIEW_W} ${VIEW_H}`}
      preserveAspectRatio="none"
      style={{ width: "100%", height: "100%", display: "block" }}
      role="img"
      aria-label="Event density"
    >
      <g transform={`translate(${PAD.left},${PAD.top})`}>
        {/* horizontal grid + y-axis value labels */}
        {GRID.map((p) => (
          <g key={`y-${p}`}>
            <line
              x1={0}
              x2={PLOT_W}
              y1={PLOT_H * p}
              y2={PLOT_H * p}
              stroke="var(--line-soft)"
              strokeWidth={1}
            />
            <text
              x={-6}
              y={PLOT_H * p + 3}
              fontSize={9}
              textAnchor="end"
              fill="var(--text-mute)"
              style={{ font: "9px/1 var(--mono)" }}
            >
              {formatAxisValue(max * (1 - p))}
            </text>
          </g>
        ))}

        <path d={areaPath} fill="var(--accent-soft)" stroke="none" />
        <path
          d={linePath}
          fill="none"
          stroke="var(--accent)"
          strokeWidth={1.6}
          strokeLinejoin="round"
        />
      </g>

      {/* x-axis time labels — absolute elapsed time (offsetNs + fraction × durationNs) */}
      {GRID.map((p, i) => {
        const absNs = offsetNs + durationNs * p;
        const totalSec = Math.floor(absNs / 1e9);
        const min = Math.floor(totalSec / 60);
        const sec = totalSec % 60;
        const label = `${String(min).padStart(2, "0")}:${String(sec).padStart(2, "0")}`;
        return (
          <text
            key={`x-${p}`}
            x={PAD.left + PLOT_W * p}
            y={VIEW_H - 6}
            fontSize={9}
            textAnchor={i === 0 ? "start" : i === GRID.length - 1 ? "end" : "middle"}
            fill="var(--text-mute)"
            style={{ font: "9px/1 var(--mono)" }}
          >
            {label}
          </text>
        );
      })}
    </svg>
  );
}
