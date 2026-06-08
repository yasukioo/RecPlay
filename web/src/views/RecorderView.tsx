import { useCallback, useEffect, useMemo, useState } from "react";

import {
  getSessionState,
  getStatsSnapshot,
  pauseRecording,
  resetSession,
  resumeRecording,
  setPluginConfig,
  startPlugin,
  startRecording,
  stopPlugin,
  stopRecording,
} from "../api/client";
import { DensityChart } from "../components/charts/DensityChart";
import { useLocaleStore } from "../stores/localeStore";
import { usePluginStore } from "../stores/pluginStore";
import { useRuntimeStore } from "../stores/runtimeStore";
import { useSessionStore } from "../stores/sessionStore";
import { useStatsStore } from "../stores/statsStore";
import { useUiStore } from "../stores/uiStore";
import type { PluginInfo, SessionState } from "../types";
import {
  buildChannelView,
  buildFallbackLogEntry,
  buildSparklineSeries,
  formatBytes,
  formatLogTimestamp,
  formatNs,
  formatPercent,
  formatRate,
} from "./workbenchModel";
import { getViewStrings } from "./viewStrings";


export function RecorderView() {
  const locale = useLocaleStore((s) => s.locale);
  const strings = getViewStrings(locale);
  const session = useSessionStore((s) => s);
  const stats = useStatsStore((s) => s);
  const plugins = usePluginStore((s) => s.plugins);
  const selectedPluginId = usePluginStore((s) => s.selectedPluginId);
  const selectPlugin = usePluginStore((s) => s.selectPlugin);
  const updatePlugin = usePluginStore((s) => s.updatePlugin);
  const runtimeChannels = useRuntimeStore((s) => s.channels);
  const setChannels = useRuntimeStore((s) => s.setChannels);
  const selectedChannelId = useRuntimeStore((s) => s.selectedChannelId);
  const selectChannel = useRuntimeStore((s) => s.selectChannel);
  const toggleChannelEnabled = useRuntimeStore((s) => s.toggleChannelEnabled);
  const eventLog = useRuntimeStore((s) => s.eventLog);
  const appendEventLog = useRuntimeStore((s) => s.appendEventLog);
  const pluginLayout = useUiStore((s) => s.pluginLayout);
  const [recordError, setRecordError] = useState<string | null>(null);
  const [busy, setBusy] = useState(false);

  // Poll session state every 500 ms while recording so duration_ns stays current.
  // WebSocket only pushes state_changed events, not periodic timeline ticks.
  useEffect(() => {
    const isRecording = session.state === "Recording" || session.state === "RecordingPaused";
    if (!isRecording) {
      // Clear waveform history so bars don't persist from a previous session.
      useStatsStore.getState().clearHistory();
      return;
    }
    const id = window.setInterval(() => {
      getSessionState()
        .then((s) => useSessionStore.getState().replaceSnapshot(s))
        .catch(() => {});
      // recordSnapshot appends a history point (drives the waveform).
      getStatsSnapshot()
        .then((s) => useStatsStore.getState().recordSnapshot(s))
        .catch(() => {});
    }, 500);
    return () => window.clearInterval(id);
  }, [session.state]);

  const channels = useMemo(() => buildChannelView(runtimeChannels, stats), [runtimeChannels, stats]);
  const selectedPlugin = plugins.find((plugin) => plugin.id === selectedPluginId) ?? null;
  const selectedChannel = channels.find((channel) => channel.id === selectedChannelId) ?? channels[0] ?? null;
  // plugin.kind is already normalised by normalizePlugin() in client.ts.
  // Calling inferPluginKind() here would ignore the kind set by the backend.
  const sourcePlugins = plugins.filter((plugin) => plugin.kind === "Source");
  const sinkPlugins = plugins.filter((plugin) => plugin.kind === "Sink");
  const enabledChannels = channels.filter((channel) => channel.enabled !== false);
  const diskFree = formatBytes(null);
  const captureBuffer = stats.ringbuf_capacity > 0 ? (stats.ringbuf_used / stats.ringbuf_capacity) * 100 : 0;

  const isRecordingActive = session.state === "Recording" || session.state === "RecordingPaused";

  // After any recording command, immediately re-fetch session state so the UI
  // reflects the new state even when the WebSocket push is slow or disconnected.
  const refreshSession = async () => {
    const snapshot = await getSessionState();
    useSessionStore.getState().replaceSnapshot(snapshot);
  };

  const handleRecordAction = async () => {
    setRecordError(null);
    setBusy(true);
    try {
      // Read the CURRENT state directly from the store — not from the render
      // closure — so a stale capture never causes stop to become start or vice versa.
      const currentState = useSessionStore.getState().state;

      if (currentState === "Recording" || currentState === "RecordingPaused") {
        await stopRecording();
        // Optimistic update: reflect Stopped in the UI immediately so the button
        // switches to "Start" even before WebSocket or polling confirms.
        useSessionStore.getState().applyTransition({ old_state: currentState, new_state: "Stopped" });
      } else {
        await resetSession().catch(() => {});
        await startRecording(buildRecordConfig(sourcePlugins));
        useSessionStore.getState().applyTransition({ old_state: currentState, new_state: "Recording" });
      }
      await refreshSession();
    } catch (error) {
      const msg = error instanceof Error ? error.message : "Command failed";
      setRecordError(msg);
      appendEventLog(buildFallbackLogEntry(`Record error: ${msg}`, "recorder"));
      try { await refreshSession(); } catch { /* ignore */ }
    } finally {
      setBusy(false);
    }
  };

  const handlePauseAction = async () => {
    setRecordError(null);
    setBusy(true);
    try {
      const currentState = useSessionStore.getState().state;
      if (currentState === "Recording") {
        await pauseRecording();
        useSessionStore.getState().applyTransition({ old_state: "Recording", new_state: "RecordingPaused" });
      } else if (currentState === "RecordingPaused") {
        await resumeRecording();
        useSessionStore.getState().applyTransition({ old_state: "RecordingPaused", new_state: "Recording" });
      }
      await refreshSession();
    } catch (error) {
      setRecordError(error instanceof Error ? error.message : "Command failed");
    } finally {
      setBusy(false);
    }
  };

  const handlePluginToggle = async (plugin: PluginInfo) => {
    const next =
      plugin.state === "active" ? await stopPlugin(plugin.id) : await startPlugin(plugin.id);
    updatePlugin(plugin.id, next);
    appendEventLog(
      buildFallbackLogEntry(
        `${plugin.name} ${plugin.state === "active" ? "stopped" : "started"}`,
        plugin.id,
      ),
    );
  };

  const handleSavePluginConfig = async (plugin: PluginInfo) => {
    if (!plugin.config_fields?.length) {
      return;
    }
    const next = await setPluginConfig(plugin.id, plugin.config_fields);
    updatePlugin(plugin.id, next);
    appendEventLog(buildFallbackLogEntry(`${plugin.name} config saved`, plugin.id));
  };

  return (
    <div className="tool">
      <div className="toolbar">
        <button
          type="button"
          disabled={busy}
          className={`btn ${session.state === "Recording" || session.state === "RecordingPaused" ? "danger" : "primary"}`}
          onClick={() => { void handleRecordAction(); }}
        >
          <span className="dot" style={{ background: "#fff", boxShadow: "0 0 6px #fff" }} />
          {busy ? "…" : getRecorderPrimaryAction(session.state, strings)}
        </button>
        <button
          type="button"
          className="btn"
          disabled={busy || !canPauseRecording(session.state)}
          onClick={() => { void handlePauseAction(); }}
        >
          {session.state === "RecordingPaused" ? `▶ ${strings.recorder.resume}` : `⏸ ${strings.recorder.pause}`}
        </button>
        <div style={{ width: 1, background: "var(--line-soft)", height: 18, margin: "0 4px" }} />
        {recordError && (
          <span
            title={recordError}
            style={{
              color: "var(--rose, #fb7185)",
              font: "11px/1 var(--mono)",
              maxWidth: 260,
              overflow: "hidden",
              textOverflow: "ellipsis",
              whiteSpace: "nowrap",
              cursor: "default",
            }}
          >
            ⚠ {recordError}
          </span>
        )}
        <span className="chip gray">
          {strings.common.session}: <b style={{ color: "var(--text)", marginLeft: 4 }}>{session.config.recordPath || "capture.rpcap"}</b>
        </span>
        <span className="chip mint">{enabledChannels.length}/{channels.length} channels</span>
        <span className="chip">{sourcePlugins.length} sources</span>
        <div style={{ flex: 1 }} />
        <span className="chip amber"><span className="dot amber" />{strings.recorder.autoSegment} 1 GiB</span>
        <span className="chip"><span className="dot mint" />zstd · lvl 6</span>
      </div>

      <div className="tool-body">
        <div className="col left">
          <div className="col-hd">
            {strings.recorder.sources}<span className="grow" />
            <span className="chip gray">{sourcePlugins.length}</span>
          </div>
          <div className="col-bd">
            <PluginList
              plugins={sourcePlugins}
              selectedPluginId={selectedPluginId}
              layout={pluginLayout}
              onSelect={selectPlugin}
              onToggle={handlePluginToggle}
            />
            <div className="tree-h" style={{ marginTop: 14 }}>{strings.recorder.sinks}</div>
            <PluginList
              plugins={sinkPlugins}
              selectedPluginId={selectedPluginId}
              layout={pluginLayout}
              onSelect={selectPlugin}
              onToggle={handlePluginToggle}
            />
          </div>
        </div>

        <div className="col stage">
          <div className="stage-top">
            <div className="viz">
              <div className="viz-hd">
                <h4>{strings.recorder.liveThroughput}</h4>
                <span className="chip mint"><span className="dot mint" />LIVE</span>
                <span className="grow" />
                <span style={{ font: "11px/1 var(--mono)", color: "var(--text-dim)" }}>window 60s · bucket 100ms</span>
              </div>
              <div className="chart-wrap" style={{ padding: 16 }}>
                {(() => {
                  const windowNs = stats.history.length > 1
                    ? (stats.history[stats.history.length - 1].timestamp - stats.history[0].timestamp) * 1_000_000
                    : 0;
                  // Offset = where the visible window starts in recording time.
                  // This makes x-axis labels show absolute elapsed time (e.g. "04:00 → 05:00")
                  // instead of always restarting from 00:00.
                  const offsetNs = Math.max(0, session.duration_ns - windowNs);
                  return (
                    <DensityChart
                      values={stats.history.map((point) => point.total_throughput_mbps)}
                      durationNs={windowNs}
                      offsetNs={offsetNs}
                    />
                  );
                })()}
              </div>
            </div>

            <div style={{ display: "flex", flexDirection: "column", gap: 12, minHeight: 0 }}>
              <div className="stats-grid">
                <StatCard label="Throughput" value={formatRate(stats.total_throughput_mbps, 2)} unit="Mbps" delta={`${stats.total_packets.toLocaleString()} pkt`} />
                <StatCard label="Packets / s" value={formatRate(enabledChannels.reduce((sum, channel) => sum + (channel.rate ?? 0), 0), 0)} unit="pkt/s" delta={`${enabledChannels.length} active`} />
                <StatCard label="Drop" value={stats.total_drops.toLocaleString()} unit="" delta={formatPercent(stats.drop_rate)} bad={stats.total_drops > 0} />
                <StatCard label="Elapsed" value={formatNs(session.duration_ns)} delta={formatNs(session.position_ns)} />
              </div>

              <div className="panel" style={{ flex: 1, minHeight: 0, display: "flex", flexDirection: "column" }}>
                <div className="panel-hd">
                  <h3>{strings.recorder.channels}</h3>
                  <span className="grow" />
                  <span className="chip gray">{channels.length}</span>
                </div>
                <div className="panel-bd" style={{ flex: 1, overflow: "auto", padding: 0 }}>
                  {channels.map((channel) => (
                    <ChannelRow
                      key={channel.id}
                      channel={channel}
                      selected={channel.id === selectedChannel?.id}
                      sparkline={buildSparklineSeries(stats.history, channel)}
                      onSelect={() => selectChannel(channel.id)}
                      onToggle={() => {
                        toggleChannelEnabled(channel.id);
                        setChannels(
                          channels.map((item) =>
                            item.id === channel.id ? { ...item, enabled: item.enabled === false } : item,
                          ),
                        );
                      }}
                    />
                  ))}
                </div>
              </div>
            </div>
          </div>

          <div className="timeline-wrap">
            <div className="tl-controls">
              <span className="tl-time">{formatNs(session.duration_ns).replace(/^00:/, "") || "00:00.000"}</span>
              <span className="chip rose"><span className="dot rose" />REC</span>
              <span className="chip mint">Disk free {diskFree}</span>
              <div style={{ flex: 1 }} />
              <span className="chip gray">{strings.recorder.captureBuffer} {formatPercent(captureBuffer)}</span>
              <span className="chip gray">{strings.recorder.queue} {stats.ringbuf_used}/{stats.ringbuf_capacity || 1024}</span>
            </div>
            <RecordWaveform
              history={stats.history}
              totalBytes={stats.total_bytes}
              totalPackets={stats.total_packets}
              isActive={isRecordingActive}
              durationNs={session.duration_ns}
            />
          </div>
        </div>

        <div className="col right">
          <div className="col-hd">{strings.recorder.inspector}</div>
          <div className="col-bd">
            <PluginInspector plugin={selectedPlugin} onSave={() => void (selectedPlugin ? handleSavePluginConfig(selectedPlugin) : Promise.resolve())} />

            <div className="panel" style={{ marginTop: 10 }}>
              <div className="panel-hd">
                <h3>{strings.recorder.eventLog}</h3>
                <span className="grow" />
                <span className="chip gray">{eventLog.length}</span>
              </div>
              <div className="panel-bd log" style={{ maxHeight: 320, overflow: "auto" }}>
                {eventLog.length === 0 && <div className="l info"><span className="t">--:--:--</span><span className="lv">INFO</span><span>No events yet</span></div>}
                {eventLog.map((entry) => (
                  <div key={entry.id} className={`l ${entry.level}`}>
                    <span className="t">{formatLogTimestamp(entry.ts)}</span>
                    <span className="lv">{entry.level.toUpperCase()}</span>
                    <span>{entry.source ? `[${entry.source}] ` : ""}{entry.message}</span>
                  </div>
                ))}
              </div>
            </div>
          </div>
        </div>
      </div>
    </div>
  );
}

function getRecorderPrimaryAction(state: SessionState, strings: ReturnType<typeof getViewStrings>): string {
  if (state === "Recording" || state === "RecordingPaused") {
    return `■ ${strings.recorder.stop}`;
  }
  return `● ${strings.recorder.start}`;
}

function canPauseRecording(state: SessionState): boolean {
  return state === "Recording" || state === "RecordingPaused";
}

function PluginList({
  plugins,
  selectedPluginId,
  layout,
  onSelect,
  onToggle,
}: {
  plugins: PluginInfo[];
  selectedPluginId: string | null;
  layout: "list" | "grid";
  onSelect: (pluginId: string) => void;
  onToggle: (plugin: PluginInfo) => Promise<void>;
}) {
  return (
    <div className={`plugin-list ${layout === "grid" ? "grid" : ""}`}>
      {plugins.map((plugin) => (
        <button
          key={plugin.id}
          type="button"
          className={`pcard ${selectedPluginId === plugin.id ? "on" : ""}`}
          onClick={() => onSelect(plugin.id)}
        >
          <div className="row">
            <span className={`dot ${plugin.state === "active" ? "mint" : plugin.state === "error" ? "rose" : "gray"}`} />
            <b>{plugin.name}</b>
          </div>
          <div className="meta">{plugin.protocol ?? plugin.kind ?? "—"} · {plugin.version}</div>
          <div className="row" style={{ justifyContent: "space-between" }}>
            <span className="chip gray">{plugin.state}</span>
            <span
              onClick={(event) => {
                event.stopPropagation();
                void onToggle(plugin);
              }}
              className="chip"
            >
              {plugin.state === "active" ? "Stop" : "Start"}
            </span>
          </div>
        </button>
      ))}
    </div>
  );
}

function ChannelRow({
  channel,
  selected,
  sparkline,
  onSelect,
  onToggle,
}: {
  channel: ReturnType<typeof buildChannelView>[number];
  selected: boolean;
  sparkline: number[];
  onSelect: () => void;
  onToggle: () => void;
}) {
  return (
    <button
      type="button"
      className={`tree-item ${selected ? "on" : ""}`}
      style={{ width: "100%", justifyContent: "space-between", borderBottom: "1px solid var(--line-soft)", padding: "8px 10px" }}
      onClick={onSelect}
    >
      <div style={{ display: "grid", gap: 4, minWidth: 0, flex: 1 }}>
        <div style={{ display: "flex", alignItems: "center", gap: 8 }}>
          <span className="dot" style={{ background: channel.color }} />
          <span style={{ color: "var(--text)" }}>{channel.name}</span>
          <span className="badge">{channel.plugin_id}</span>
        </div>
        <div style={{ display: "flex", alignItems: "center", gap: 10, color: "var(--text-dim)", font: "11px/1 var(--mono)" }}>
          <span>{formatRate(channel.rate, 1)} Mbps</span>
          <span>{channel.protocol}</span>
          <span>{formatBytes(channel.bytes)}</span>
        </div>
      </div>
      <svg viewBox="0 0 100 24" width="100" height="24" aria-hidden="true">
        <path d={buildLinePath(sparkline, 100, 24)} fill="none" stroke={channel.color} strokeWidth="1.5" />
      </svg>
      <span
        className={`chip ${channel.enabled === false ? "gray" : "mint"}`}
        onClick={(event) => {
          event.stopPropagation();
          onToggle();
        }}
      >
        {channel.enabled === false ? "Off" : "On"}
      </span>
    </button>
  );
}

function PluginInspector({ plugin, onSave }: { plugin: PluginInfo | null; onSave: () => void }) {
  if (!plugin) {
    return (
      <div className="panel">
        <div className="panel-hd"><h3>Plugin</h3></div>
        <div className="panel-bd" style={{ color: "var(--text-mute)", font: "11px/1.5 var(--mono)" }}>
          Select a plugin to inspect its runtime config.
        </div>
      </div>
    );
  }

  return (
    <div className="panel">
      <div className="panel-hd">
        <h3>{plugin.name}</h3>
        <span className="grow" />
        <span className="chip gray">{plugin.kind ?? "Service"}</span>
      </div>
      <div className="panel-bd inspector" style={{ display: "grid", gap: 6 }}>
        <div className="row2"><span className="k">id</span><span className="v">{plugin.id}</span></div>
        <div className="row2"><span className="k">version</span><span className="v">{plugin.version}</span></div>
        <div className="row2"><span className="k">state</span><span className="v">{plugin.state}</span></div>
        {plugin.config_fields?.map((field) => (
          <div className="row2" key={field.key}>
            <span className="k">{field.label}</span>
            <span className="v">{field.value}</span>
          </div>
        ))}
        <div style={{ display: "flex", justifyContent: "flex-end", marginTop: 8 }}>
          <button type="button" className="btn" onClick={onSave}>Save</button>
        </div>
      </div>
    </div>
  );
}

function StatCard({
  label,
  value,
  unit,
  delta,
  bad,
}: {
  label: string;
  value: string;
  unit?: string;
  delta: string;
  bad?: boolean;
}) {
  return (
    <div className="stat">
      <div className="k">{label}</div>
      <div className="v">
        {value}
        {unit ? <span className="u">{unit}</span> : null}
      </div>
      <div className={`d ${bad ? "bad" : ""}`}>{delta}</div>
    </div>
  );
}

// ---------------------------------------------------------------------------
// RecordWaveform helpers
// ---------------------------------------------------------------------------

const MAX_SEGMENT_BYTES = 4 * 1024 * 1024 * 1024;
const WAVEFORM_SLOTS = 120;
const ZOOM_STEPS = [1, 2, 4, 8, 16, 32] as const;
type ZoomStep = typeof ZOOM_STEPS[number];

/** Return a "nice" tick interval in seconds for ~targetCount ticks over totalSec. */
function niceTickSec(totalSec: number, targetCount = 5): number {
  if (totalSec <= 0) return 1;
  const ideal = totalSec / targetCount;
  for (const c of [0.5, 1, 2, 5, 10, 15, 30, 60, 120, 300, 600, 1800, 3600]) {
    if (c >= ideal) return c;
  }
  return 3600;
}

/** Format seconds as "5s", "1m30s", "2m", etc. */
function fmtSec(sec: number): string {
  const s = Math.round(sec);
  if (s < 60) return `${s}s`;
  const m = Math.floor(s / 60);
  const r = s % 60;
  return r > 0 ? `${m}m${r}s` : `${m}m`;
}

// ---------------------------------------------------------------------------
// RecordWaveform — live recording waveform.
//
//  • Bars drawn left→right from recording start
//  • Height = per-window byte delta (data volume)
//  • Background fill = total bytes / 4 GiB
//  • Ticks = absolute recording elapsed time, auto-scaled to total duration
//  • Mouse-wheel zooms in/out; bars aggregate at higher zoom levels
// ---------------------------------------------------------------------------
function RecordWaveform({
  history,
  totalBytes,
  totalPackets,
  isActive,
  durationNs,
}: {
  history: import("../types").StatsHistoryPoint[];
  totalBytes: number;
  totalPackets: number;
  isActive: boolean;
  durationNs: number;
}) {
  const [zoom, setZoom] = useState<ZoomStep>(1);

  const handleWheel = useCallback((e: React.WheelEvent) => {
    e.preventDefault();
    setZoom((z) => {
      const idx = ZOOM_STEPS.indexOf(z);
      if (e.deltaY > 0) return ZOOM_STEPS[Math.min(idx + 1, ZOOM_STEPS.length - 1)];
      return ZOOM_STEPS[Math.max(idx - 1, 0)];
    });
  }, []);

  // Per-sample byte deltas
  const rawDeltas = useMemo(
    () => history.map((p, i) =>
      i === 0 ? p.total_bytes : Math.max(0, p.total_bytes - history[i - 1].total_bytes),
    ),
    [history],
  );

  // Aggregate rawDeltas in groups of `zoom` (take max of each group)
  const bars = useMemo(() => {
    const out: number[] = [];
    for (let i = 0; i < rawDeltas.length; i += zoom) {
      out.push(Math.max(0, ...rawDeltas.slice(i, i + zoom)));
    }
    return out;
  }, [rawDeltas, zoom]);

  const maxBar = useMemo(() => Math.max(1, ...bars), [bars]);
  const fillPct = Math.min(100, (totalBytes / MAX_SEGMENT_BYTES) * 100);

  // Each aggregated bar represents `zoom` samples; each sample ≈ 500 ms.
  const avgIntervalMs = history.length > 1
    ? (history[history.length - 1].timestamp - history[0].timestamp) / (history.length - 1)
    : 500;
  const msPerBar = zoom * avgIntervalMs;

  // Total recording duration (from session) or fall back to visible window
  const totalSec = durationNs > 0 ? durationNs / 1e9 : (bars.length * msPerBar) / 1000;

  // Ticks: auto-scaled to total recording duration, anchored to absolute time.
  // Visible window starts at (totalSec - visible bars * sec/bar) from recording origin.
  const visibleSec = (bars.length * msPerBar) / 1000;
  const visibleStartSec = Math.max(0, totalSec - visibleSec);
  const tickInterval = niceTickSec(totalSec, 5);

  const ticks = useMemo(() => {
    const result: { label: string; leftPct: number }[] = [];
    const firstTick = Math.ceil(visibleStartSec / tickInterval) * tickInterval;
    for (let t = firstTick; t <= totalSec + 0.001; t += tickInterval) {
      const relSec = t - visibleStartSec;
      const slot = (relSec * 1000) / msPerBar; // visual slot within bar array
      const leftPct = (slot / WAVEFORM_SLOTS) * 100;
      if (leftPct < -1 || leftPct > 101) continue;
      result.push({ label: fmtSec(t), leftPct: Math.max(0, leftPct) });
    }
    return result;
  }, [visibleStartSec, totalSec, tickInterval, msPerBar]);

  const scrubPct = bars.length > 0
    ? ((bars.length - 0.5) / WAVEFORM_SLOTS) * 100
    : null;

  const barWidthPct = (zoom / WAVEFORM_SLOTS) * 100; // visual width per bar
  const lastMbps = history.length > 0 ? history[history.length - 1].total_throughput_mbps : 0;

  return (
    <div style={{ display: "flex", flexDirection: "column", gap: 4 }}>
      {/* main track */}
      <div className="tl-track" onWheel={handleWheel} style={{ cursor: "crosshair" }}>
        <div className="tl-grid" />

        {/* bytes-progress fill (rose colour) */}
        <div
          className="tl-progress"
          style={{
            width: fillPct > 0 ? `${fillPct}%` : isActive ? "2px" : "0",
            background: "linear-gradient(180deg,color-mix(in srgb,var(--rose) 24%,transparent),color-mix(in srgb,var(--rose) 6%,transparent))",
            borderRight: "1px solid color-mix(in srgb,var(--rose) 70%,transparent)",
            boxShadow: "2px 0 14px color-mix(in srgb,var(--rose) 45%,transparent)",
          }}
        />

        <div className="tl-lanes">
          <div className="tl-lane">
            {bars.map((delta, i) => {
              const leftPct = (i / WAVEFORM_SLOTS) * 100;
              const halfH = (delta / maxBar) * 45;
              const opacity = 0.35 + (i / Math.max(1, bars.length - 1)) * 0.65;
              return (
                <div
                  key={i}
                  className="tl-pkt rose"
                  style={{
                    left: `${leftPct}%`,
                    width: `calc(${barWidthPct}% - 1px)`,
                    top: `${50 - halfH}%`,
                    bottom: `${50 - halfH}%`,
                    opacity,
                  }}
                />
              );
            })}
          </div>
        </div>

        {isActive && scrubPct !== null && (
          <div className="tl-scrub" style={{ left: `${scrubPct}%` }} />
        )}
      </div>

      {/* ticks row + stats */}
      <div style={{
        display: "flex", alignItems: "flex-start", justifyContent: "space-between",
        font: "10px/1 var(--mono)", color: "var(--text-mute)",
      }}>
        {/* absolutely-positioned time ticks anchored to recording elapsed time */}
        <div style={{ position: "relative", flex: 1, height: 14 }}>
          {ticks.map(({ label, leftPct }, i) => (
            <span
              key={i}
              style={{
                position: "absolute",
                left: `${leftPct}%`,
                transform: leftPct < 5 ? "none"
                  : leftPct > 95 ? "translateX(-100%)"
                  : "translateX(-50%)",
                whiteSpace: "nowrap",
              }}
            >
              {label}
            </span>
          ))}
        </div>

        {/* stats + zoom badge */}
        <div style={{ display: "flex", gap: 8, color: "var(--text-dim)", marginLeft: 16, flexShrink: 0 }}>
          {zoom > 1 && (
            <span style={{ color: "var(--amber,#ffb347)" }}>{zoom}×</span>
          )}
          <span style={{ color: "var(--text)" }}>
            {totalBytes > 0 ? formatBytes(totalBytes) : "— B"} / 4 GiB
          </span>
          <span style={{ opacity: 0.4 }}>·</span>
          <span>{totalPackets.toLocaleString()} pkt</span>
          {lastMbps > 0 && (
            <>
              <span style={{ opacity: 0.4 }}>·</span>
              <span style={{ color: "var(--mint,#5eead4)" }}>
                {lastMbps < 1
                  ? `${(lastMbps * 1000).toFixed(0)} kbps`
                  : `${lastMbps.toFixed(2)} Mbps`}
              </span>
            </>
          )}
        </div>
      </div>
    </div>
  );
}

// Build the recording request body from the first source plugin's config fields.
// Fields whose names end in "port", "id", or "buf" are coerced to numbers so the
// C++ JSON parser doesn't reject them as wrong-type strings.
function buildRecordConfig(sourcePlugins: PluginInfo[]): Record<string, unknown> {
  const config: Record<string, unknown> = { output_path: "capture.rpcap" };
  const src = sourcePlugins[0];
  if (src?.config_fields?.length) {
    const protocolConfig: Record<string, unknown> = {};
    for (const { key, value } of src.config_fields) {
      if (/^(port|channel_id|recv_buf)$/.test(key) && /^\d+$/.test(value)) {
        protocolConfig[key] = Number(value);
      } else {
        protocolConfig[key] = value;
      }
    }
    config.protocol_config = protocolConfig;
  }
  return config;
}

function buildLinePath(values: number[], width: number, height: number): string {
  if (values.length === 0) {
    return `M0 ${height}`;
  }
  const max = Math.max(...values, 1);
  return values
    .map((value, index) => {
      const x = values.length === 1 ? width / 2 : (index / (values.length - 1)) * width;
      const y = height - (value / max) * (height - 4) - 2;
      return `${index === 0 ? "M" : "L"}${x.toFixed(2)} ${y.toFixed(2)}`;
    })
    .join(" ");
}

