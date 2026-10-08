import { useMemo, useRef, useState } from "react";

import { getTranslations } from "../../i18n";
import { seekPlayback } from "../../api/client";
import { echarts, ReactEChartsCore } from "../../charts/echarts";
import { useLocaleStore } from "../../stores/localeStore";
import { useSessionStore } from "../../stores/sessionStore";
import { useStatsStore } from "../../stores/statsStore";
import {
  buildDerivedMarkers,
  calculateTimelineSeekTimestamp,
  createZoomWindow,
  formatTimelineTimestamp,
  getLoopRegionStyle,
  TIMELINE_FOCUS_ID,
} from "./timelineModel";
import {
  getDisplayedSeekPositionNs,
  getPositionPercent,
} from "../seekModel";

export function Timeline() {
  const locale = useLocaleStore((s) => s.locale);
  const positionNs = useSessionStore((s) => s.position_ns);
  const durationNs = useSessionStore((s) => s.duration_ns);
  const loopStartNs = useSessionStore((s) => s.loop_start_ns);
  const loopEndNs = useSessionStore((s) => s.loop_end_ns);
  const history = useStatsStore((s) => s.history);
  const [zoomLevel, setZoomLevel] = useState(1);
  const [busy, setBusy] = useState(false);
  const [previewPositionNs, setPreviewPositionNs] = useState<number | null>(null);
  const chartRef = useRef<HTMLButtonElement | null>(null);
  const dragPointerIdRef = useRef<number | null>(null);
  const t = getTranslations(locale);

  const zoomWindow = useMemo(
    () => createZoomWindow(history.length, zoomLevel),
    [history.length, zoomLevel],
  );
  const visibleHistory = history.slice(zoomWindow.start, zoomWindow.end);
  const markers = useMemo(
    () => buildDerivedMarkers(visibleHistory, durationNs, locale),
    [durationNs, locale, visibleHistory],
  );
  const loopRegionStyle = useMemo(
    () => getLoopRegionStyle(loopStartNs, loopEndNs, durationNs),
    [durationNs, loopEndNs, loopStartNs],
  );

  const option = {
    grid: { top: 8, bottom: 20, left: 40, right: 16 },
    xAxis: {
      type: "category" as const,
      data: visibleHistory.map((_, i) => i + zoomWindow.start),
      show: false,
    },
    yAxis: {
      type: "value" as const,
      show: false,
    },
    series: [
      {
        type: "line",
        data: visibleHistory.map((p) => p.total_throughput_mbps),
        smooth: true,
        symbol: "none",
        areaStyle: { color: "rgba(79,195,247,0.2)" },
        lineStyle: { color: "#4fc3f7", width: 1.5 },
      },
    ],
    tooltip: { show: false },
  };

  const displayedPositionNs = useMemo(
    () => getDisplayedSeekPositionNs(positionNs, previewPositionNs, durationNs),
    [durationNs, positionNs, previewPositionNs],
  );
  const progressPct = getPositionPercent(displayedPositionNs, durationNs);

  const getTimestampFromClientX = (clientX: number) => {
    if (!chartRef.current) {
      return 0;
    }

    const rect = chartRef.current.getBoundingClientRect();
    return calculateTimelineSeekTimestamp({
      clientX,
      left: rect.left,
      width: rect.width,
      durationNs,
    });
  };

  const commitSeek = async (timestampNs: number) => {
    if (busy || durationNs <= 0) {
      setPreviewPositionNs(null);
      return;
    }

    try {
      setBusy(true);
      setPreviewPositionNs(timestampNs);
      await seekPlayback(timestampNs);
    } finally {
      setBusy(false);
      setPreviewPositionNs(null);
    }
  };

  return (
    <div
      id={TIMELINE_FOCUS_ID}
      tabIndex={0}
      role="region"
      aria-label={t.timeline.ariaLabel}
      className="rounded border border-hmi-border bg-hmi-surface p-2 focus:outline-none focus-visible:ring-2 focus-visible:ring-hmi-accent/70"
    >
      <div className="mb-2 flex items-center justify-between">
        <div className="text-xs uppercase tracking-wide text-hmi-text-muted">
          {t.timeline.title}
        </div>
        <div className="flex items-center gap-2">
          <button
            type="button"
            onClick={() => setZoomLevel((current) => Math.max(1, current / 2))}
            className="rounded border border-hmi-border bg-hmi-surface-alt px-2 py-1 text-xs text-hmi-text hover:bg-hmi-border"
          >
            -
          </button>
          <span className="min-w-10 text-center text-xs text-hmi-text-muted">{zoomLevel}x</span>
          <button
            type="button"
            onClick={() => setZoomLevel((current) => Math.min(8, current * 2))}
            className="rounded border border-hmi-border bg-hmi-surface-alt px-2 py-1 text-xs text-hmi-text hover:bg-hmi-border"
          >
            +
          </button>
        </div>
      </div>

      <button
        ref={chartRef}
        type="button"
        disabled={busy || durationNs <= 0}
        className="relative block w-full touch-none rounded text-left disabled:cursor-not-allowed"
        onPointerDown={(event) => {
          if (busy || durationNs <= 0) {
            return;
          }

          dragPointerIdRef.current = event.pointerId;
          event.currentTarget.setPointerCapture(event.pointerId);
          setPreviewPositionNs(getTimestampFromClientX(event.clientX));
        }}
        onPointerMove={(event) => {
          if (dragPointerIdRef.current !== event.pointerId) {
            return;
          }

          setPreviewPositionNs(getTimestampFromClientX(event.clientX));
        }}
        onPointerUp={(event) => {
          if (dragPointerIdRef.current !== event.pointerId) {
            return;
          }

          dragPointerIdRef.current = null;
          if (event.currentTarget.hasPointerCapture(event.pointerId)) {
            event.currentTarget.releasePointerCapture(event.pointerId);
          }
          void commitSeek(getTimestampFromClientX(event.clientX));
        }}
        onPointerCancel={() => {
          dragPointerIdRef.current = null;
          setPreviewPositionNs(null);
        }}
        title={t.timeline.clickToSeek}
      >
        <ReactEChartsCore
          echarts={echarts}
          option={option}
          style={{ height: "64px" }}
          opts={{ renderer: "svg" }}
        />
        {loopRegionStyle && (
          <div
            className="pointer-events-none absolute top-0 bottom-0 border-x border-hmi-accent/50 bg-hmi-accent/10"
            style={loopRegionStyle}
          />
        )}
        <div
          className="pointer-events-none absolute top-0 bottom-0 w-px bg-hmi-accent"
          style={{ left: `${progressPct}%` }}
        />

        {markers.map((marker) => {
          const left = `${getPositionPercent(marker.timestamp_ns, durationNs)}%`;
          return (
            <div
              key={`${marker.timestamp_ns}-${marker.label}`}
              className="pointer-events-none absolute top-1/2 -translate-y-1/2"
              style={{ left }}
            >
              <div className="h-3 w-3 rounded-full border border-hmi-danger/60 bg-hmi-danger/30" />
            </div>
          );
        })}
      </button>

      {markers.length > 0 && (
        <div className="mt-2 flex flex-wrap gap-2">
          {markers.map((marker) => (
            <span
              key={`legend-${marker.timestamp_ns}-${marker.label}`}
              className="rounded border border-hmi-danger/30 bg-hmi-danger/10 px-2 py-1 text-xs text-hmi-danger"
            >
              {formatTimelineTimestamp(marker.timestamp_ns)} {marker.label}
            </span>
          ))}
        </div>
      )}
      {loopRegionStyle && loopStartNs !== null && loopEndNs !== null && (
        <div className="mt-2 text-xs text-hmi-accent">
          {t.timeline.loop}: {formatTimelineTimestamp(loopStartNs)} - {formatTimelineTimestamp(loopEndNs)}
        </div>
      )}
    </div>
  );
}
