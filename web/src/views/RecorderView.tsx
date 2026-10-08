import { useMemo } from "react";

import {
  pauseRecording,
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
  inferPluginKind,
} from "./workbenchModel";
import { getViewStrings } from "./viewStrings";

const DEFAULT_RECORD_CONFIG = {
  output_path: "capture.rpcap",
  protocols: ["UDP", "TCP"],
};

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

  const channels = useMemo(() => buildChannelView(runtimeChannels, stats), [runtimeChannels, stats]);
  const selectedPlugin = plugins.find((plugin) => plugin.id === selectedPluginId) ?? null;
  const selectedChannel = channels.find((channel) => channel.id === selectedChannelId) ?? channels[0] ?? null;
  const sourcePlugins = plugins.filter((plugin) => inferPluginKind(plugin) === "Source");
  const sinkPlugins = plugins.filter((plugin) => inferPluginKind(plugin) === "Sink");
  const enabledChannels = channels.filter((channel) => channel.enabled !== false);
  const diskFree = formatBytes(null);
  const captureBuffer = stats.ringbuf_capacity > 0 ? (stats.ringbuf_used / stats.ringbuf_capacity) * 100 : 0;

  const handleRecordAction = async () => {
    if (session.state === "Recording" || session.state === "RecordingPaused") {
      await stopRecording();
      return;
    }
    await startRecording(DEFAULT_RECORD_CONFIG);
  };

  const handlePauseAction = async () => {
    if (session.state === "Recording") {
      await pauseRecording();
    } else if (session.state === "RecordingPaused") {
      await resumeRecording();
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
          className={`btn ${session.state === "Recording" || session.state === "RecordingPaused" ? "danger" : "primary"}`}
          onClick={() => void handleRecordAction()}
        >
          <span className="dot" style={{ background: "#fff", boxShadow: "0 0 6px #fff" }} />
          {getRecorderPrimaryAction(session.state, strings)}
        </button>
        <button type="button" className="btn" disabled={!canPauseRecording(session.state)} onClick={() => void handlePauseAction()}>
          {session.state === "RecordingPaused" ? `▶ ${strings.recorder.resume}` : `⏸ ${strings.recorder.pause}`}
        </button>
        <div style={{ width: 1, background: "var(--line-soft)", height: 18, margin: "0 4px" }} />
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
                <DensityChart
                  values={stats.history.map((point) => point.total_throughput_mbps)}
                  durationNs={
                    stats.history.length > 1
                      ? (stats.history[stats.history.length - 1].timestamp - stats.history[0].timestamp) * 1_000_000
                      : 0
                  }
                />
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
              <span className="chip">file #1 · {formatBytes(stats.disk_queue_bytes)} / 4 GiB</span>
              <span className="chip mint">Disk free {diskFree}</span>
              <div style={{ flex: 1 }} />
              <span className="chip gray">{strings.recorder.captureBuffer} {formatPercent(captureBuffer)}</span>
              <span className="chip gray">{strings.recorder.queue} {stats.ringbuf_used}/{stats.ringbuf_capacity || 1024}</span>
            </div>
            <div className="tl-track compact" style={{ height: 32 }}>
              <div className="tl-grid" />
              <div className="tl-progress" style={{ width: `${Math.min(100, captureBuffer)}%` }} />
            </div>
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
          <div className="meta">{plugin.protocol ?? inferPluginKind(plugin)} · {plugin.version}</div>
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
        <span className="chip gray">{plugin.kind ?? inferPluginKind(plugin)}</span>
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

