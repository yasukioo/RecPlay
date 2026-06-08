import { useEffect, useMemo } from "react";

import {
  getPlaybackDensity,
  openPlaybackFile,
  pausePlayback,
  playPlayback,
  seekPlayback,
  setPlaybackLoop,
  setPlaybackSpeed,
  setReplayTargets,
  stopPlayback,
} from "../api/client";
import { DensityChart } from "../components/charts/DensityChart";
import { useLocaleStore } from "../stores/localeStore";
import { useRuntimeStore } from "../stores/runtimeStore";
import { useSessionStore } from "../stores/sessionStore";
import { useStatsStore } from "../stores/statsStore";
import { useUiStore } from "../stores/uiStore";
import type { ReplayTarget, TimelineMarker } from "../types";
import {
  buildFallbackPacket,
  buildReplayTargets,
  buildTimelineMarkers,
  formatBytes,
  formatFileMeta,
  formatNs,
  formatRate,
} from "./workbenchModel";
import { getViewStrings } from "./viewStrings";

const RATES = [0.25, 0.5, 1, 2, 4, 8] as const;

export function PlayerView() {
  const locale = useLocaleStore((s) => s.locale);
  const strings = getViewStrings(locale);
  const session = useSessionStore((s) => s);
  const setLoopRegion = useSessionStore((s) => s.setLoopRegion);
  const stats = useStatsStore((s) => s);
  const recordingFiles = useRuntimeStore((s) => s.recordingFiles);
  const selectedFileName = useRuntimeStore((s) => s.selectedFileName);
  const selectFile = useRuntimeStore((s) => s.selectFile);
  const channels = useRuntimeStore((s) => s.channels);
  const selectedChannelId = useRuntimeStore((s) => s.selectedChannelId);
  const selectChannel = useRuntimeStore((s) => s.selectChannel);
  const toggleChannelEnabled = useRuntimeStore((s) => s.toggleChannelEnabled);
  const markers = useRuntimeStore((s) => s.markers);
  const replayTargets = useRuntimeStore((s) => s.replayTargets);
  const setRuntimeReplayTargets = useRuntimeStore((s) => s.setReplayTargets);
  const currentPacket = useRuntimeStore((s) => s.currentPacket);
  const setCurrentPacket = useRuntimeStore((s) => s.setCurrentPacket);
  const density = useRuntimeStore((s) => s.density);
  const setDensity = useRuntimeStore((s) => s.setDensity);
  const rateUi = useUiStore((s) => s.rateUi);
  const tlStyle = useUiStore((s) => s.timelineStyle);

  const files = recordingFiles;
  const selectedFile = files.find((file) => file.name === selectedFileName) ?? files[0] ?? null;
  const activeChannels = channels.filter((channel) => channel.enabled !== false);
  const selectedChannel = channels.find((channel) => channel.id === selectedChannelId) ?? activeChannels[0] ?? null;
  const effectivePacket = currentPacket ?? buildFallbackPacket(selectedChannel, session.position_ns, session.duration_ns);
  const effectiveMarkers = useMemo(
    () => buildTimelineMarkers(markers, stats.history, session.duration_ns),
    [markers, session.duration_ns, stats.history],
  );
  const effectiveReplayTargets = useMemo(
    () => buildReplayTargets(channels, replayTargets),
    [channels, replayTargets],
  );
  const fileMeta = formatFileMeta(selectedFile);

  // Fetch event density for whatever file is already open (covers initial load).
  useEffect(() => {
    let cancelled = false;
    void getPlaybackDensity().then((result) => {
      if (!cancelled) {
        setDensity(result);
      }
    });
    return () => {
      cancelled = true;
    };
  }, [setDensity]);

  const handleOpenFile = async (fileName: string) => {
    const file = files.find((entry) => entry.name === fileName);
    selectFile(fileName);
    await openPlaybackFile(file?.path ?? fileName);
    setDensity(await getPlaybackDensity());
  };

  const handlePlayPause = async () => {
    if (session.state === "Playing") {
      await pausePlayback();
    } else {
      await playPlayback(session.speed || 1);
    }
  };

  const handleRateChange = async (rate: number) => {
    await setPlaybackSpeed(rate);
    useSessionStore.setState({ speed: rate });
  };

  const handleSeek = async (timestampNs: number) => {
    await seekPlayback(timestampNs);
    useSessionStore.setState({ position_ns: timestampNs });
    setCurrentPacket(buildFallbackPacket(selectedChannel, timestampNs, session.duration_ns));
  };

  const handleLoop = async (startNs: number, endNs: number) => {
    await setPlaybackLoop(startNs, endNs);
    setLoopRegion(startNs, endNs);
  };

  const handleReplayTargetToggle = async (target: ReplayTarget) => {
    const nextTargets = effectiveReplayTargets.map((item) =>
      item.id === target.id ? { ...item, enabled: !item.enabled } : item,
    );
    setRuntimeReplayTargets(nextTargets);
    await setReplayTargets(nextTargets);
  };

  const trackMarkers = effectiveMarkers.map((marker) => ({
    ...marker,
    left: `${session.duration_ns > 0 ? (marker.timestamp_ns / session.duration_ns) * 100 : 0}%`,
  }));

  const progressPct = session.duration_ns > 0 ? (session.position_ns / session.duration_ns) * 100 : 0;
  const loopLeft = session.loop_start_ns != null && session.duration_ns > 0 ? (session.loop_start_ns / session.duration_ns) * 100 : null;
  const loopWidth =
    session.loop_start_ns != null &&
    session.loop_end_ns != null &&
    session.duration_ns > 0
      ? ((session.loop_end_ns - session.loop_start_ns) / session.duration_ns) * 100
      : null;

  return (
    <div className="tool">
      <div className="toolbar">
        <button type="button" className="btn primary" onClick={() => void handlePlayPause()}>
          {session.state === "Playing" ? `⏸ ${strings.player.pause}` : `▶ ${strings.player.play}`}
        </button>
        <button type="button" className="btn" onClick={() => void handleSeek(Math.max(0, session.position_ns - 1_000_000))}>⏮ {strings.player.previousPacket}</button>
        <button type="button" className="btn" onClick={() => void handleSeek(Math.min(session.duration_ns, session.position_ns + 1_000_000))}>⏭ {strings.player.nextPacket}</button>
        <button type="button" className="btn" onClick={() => void handleSeek(0)}>↺ {strings.player.reset}</button>
        <div style={{ width: 1, background: "var(--line-soft)", height: 22, margin: "0 6px" }} />
        <span className="chip mint"><span className="dot mint" />{selectedFile?.name ?? "— .rpcap"}</span>
        <span className="chip gray">
          {formatBytes(selectedFile?.size)} · {formatNs(selectedFile?.duration_ns)} · {selectedFile?.channels ?? activeChannels.length} ch
        </span>
        <div style={{ flex: 1 }} />
        <span className="chip"><span className="dot" />loop {session.loop_start_ns != null ? `${formatNs(session.loop_start_ns)}–${formatNs(session.loop_end_ns)}` : "—"}</span>
        <button type="button" className="btn ghost">{strings.player.inject}</button>
        <button type="button" className="btn ghost icon" title={strings.player.bookmark}>✦</button>
      </div>

      <div className="tool-body">
        <div className="col left">
          <div className="col-hd">
            {strings.player.captures}<span className="grow" />
            <span className="chip gray">{files.length}</span>
          </div>
          <div className="col-bd">
            {files.map((file) => (
              <button key={file.name} type="button" className={`file-row ${selectedFile?.name === file.name ? "on" : ""}`} onClick={() => void handleOpenFile(file.name)}>
                <span className="nm"><span className="dot mint" />{file.name}</span>
                <span className="sz">{formatBytes(file.size)}</span>
                <span className="dur">{formatNs(file.duration_ns)}</span>
                <span className="dur">{file.channels ?? "—"} ch</span>
              </button>
            ))}

            <div className="tree-h" style={{ marginTop: 12 }}>{strings.player.channelMap}</div>
            {channels.map((channel) => (
              <button key={channel.id} type="button" className={`tree-item ${selectedChannel?.id === channel.id ? "on" : ""}`} style={{ width: "100%" }} onClick={() => selectChannel(channel.id)}>
                <span className="dot" style={{ background: channel.color }} />
                <span>{channel.name ?? channel.topic}</span>
                <span className="badge">{channel.enabled === false ? "Off" : "On"}</span>
                <span
                  className="chip gray"
                  onClick={(event) => {
                    event.stopPropagation();
                    toggleChannelEnabled(channel.id);
                  }}
                >
                  {channel.enabled === false ? "Enable" : "Disable"}
                </span>
              </button>
            ))}

            <div className="tree-h" style={{ marginTop: 12 }}>{strings.player.markers}</div>
            {effectiveMarkers.map((marker) => (
              <button key={`${marker.timestamp_ns}-${marker.label}`} type="button" className="tree-item" style={{ width: "100%" }} onClick={() => void handleSeek(marker.timestamp_ns)}>
                <span className="dot amber" />
                <span>{marker.label}</span>
                <span className="badge">{formatNs(marker.timestamp_ns)}</span>
              </button>
            ))}
          </div>
        </div>

        <div className="col stage">
          <div className="stage-top">
            <div className="viz">
              <div className="viz-hd">
                <h4>{strings.player.eventDensity}</h4>
                <span className="grow" />
                <span style={{ font: "11px/1 var(--mono)", color: "var(--text-dim)" }}>{activeChannels.length} channels · bucket 100ms</span>
              </div>
              <div className="chart-wrap" style={{ padding: 16 }}>
                <DensityChart values={density.total} durationNs={density.duration_ns} />
              </div>
            </div>

            <div style={{ display: "flex", flexDirection: "column", gap: 12, minHeight: 0 }}>
              <div className="stats-grid">
                <StatCard label={strings.timeline.position} value={formatNs(session.position_ns)} delta={formatNs(session.duration_ns)} />
                <StatCard label={strings.timeline.rate} value={formatRate(session.speed, 2)} unit="×" delta={session.state} />
                <StatCard label={strings.timeline.bytes} value={formatBytes(selectedFile?.size)} delta={`${stats.total_packets.toLocaleString()} pkt`} />
                <StatCard label={strings.timeline.drift} value={formatRate(stats.write_latency_p99_ms, 1)} unit="ms" delta={formatRate(stats.drop_rate, 1)} />
              </div>

              <div className="panel" style={{ flex: 1, minHeight: 0, display: "flex", flexDirection: "column" }}>
                <div className="panel-hd">
                  <h3>{strings.player.fileMetadata}</h3>
                  <span className="grow" />
                  <span className="chip gray">{selectedFile?.codec ?? ".rpcap v1.0"}</span>
                </div>
                <div className="panel-bd inspector" style={{ padding: "10px 12px" }}>
                  {fileMeta.map((row) => (
                    <div className="row2" key={row.key}>
                      <span className="k">{row.key}</span>
                      <span className="v">{row.value}</span>
                    </div>
                  ))}
                </div>
              </div>
            </div>
          </div>

          <div className="timeline-wrap">
            <div className="tl-controls">
              <span className="tl-time">{formatNs(session.position_ns).replace(/^00:/, "")}</span>
              <span style={{ color: "var(--text-mute)", font: "11px/1 var(--mono)" }}>/</span>
              <span style={{ color: "var(--text-dim)", font: "600 14px/1 var(--mono)" }}>{formatNs(session.duration_ns).replace(/^00:/, "")}</span>
              <div style={{ width: 1, background: "var(--line-soft)", height: 18, margin: "0 6px" }} />

              {rateUi === "slider" ? (
                <div className="tl-slider">
                  <span style={{ font: "10px/1 var(--mono)", color: "var(--text-mute)" }}>0.25×</span>
                  <input
                    type="range"
                    className="rng"
                    min={0}
                    max={RATES.length - 1}
                    step={1}
                    value={Math.max(0, RATES.indexOf((session.speed as typeof RATES[number]) || 1))}
                    onChange={(event) => void handleRateChange(RATES[Number(event.target.value)] ?? 1)}
                  />
                  <span style={{ font: "10px/1 var(--mono)", color: "var(--text-mute)" }}>8×</span>
                  <span className="val">{session.speed}×</span>
                </div>
              ) : rateUi === "knob" ? (
                <button type="button" className="tl-knob-wrap" onClick={() => void handleRateChange(RATES[(RATES.indexOf((session.speed as typeof RATES[number]) || 1) + 1) % RATES.length] ?? 1)}>
                  <div className="tl-knob" style={{ ["--ang" as string]: `${RATES.indexOf((session.speed as typeof RATES[number]) || 1) * 40 - 80}deg` }} />
                  <small>{session.speed}×</small>
                </button>
              ) : (
                <div className="tl-rate">
                  {RATES.map((rate) => (
                    <button key={rate} type="button" className={session.speed === rate ? "on" : ""} onClick={() => void handleRateChange(rate)}>
                      {rate}×
                    </button>
                  ))}
                </div>
              )}

              <button
                type="button"
                className="btn ghost"
                onClick={() => void handleLoop(Math.max(0, session.position_ns - 5_000_000_000), Math.min(session.duration_ns, session.position_ns + 5_000_000_000))}
              >
                ◌ {strings.player.setLoop}
              </button>
              <span className="chip gray">{strings.timeline.shiftLoopHint}</span>
              <div style={{ flex: 1 }} />
            </div>

            <button
              type="button"
              className={"tl-track " + tlStyle}
              style={{ textAlign: "left" }}
              title={strings.timeline.clickHint}
              onClick={(event) => {
                const rect = event.currentTarget.getBoundingClientRect();
                const ratio = Math.min(1, Math.max(0, (event.clientX - rect.left) / rect.width));
                void handleSeek(Math.round(session.duration_ns * ratio));
              }}
            >
              <div className="tl-grid" />
              <div className="tl-progress" style={{ width: `${progressPct}%` }} />
              {loopLeft != null && loopWidth != null ? <div className="tl-loop" style={{ left: `${loopLeft}%`, width: `${loopWidth}%` }} /> : null}
              {tlStyle === "detail" ? (
                <div className="tl-lanes">
                  {activeChannels.map((channel) => (
                    <div className="tl-lane" key={channel.id}>
                      <span className="lname">{channel.name ?? channel.id}</span>
                      {trackMarkers.map((marker) => (
                        <span key={`${channel.id}-${marker.timestamp_ns}`} className="tl-pkt" style={{ left: marker.left, background: channel.color }} />
                      ))}
                    </div>
                  ))}
                </div>
              ) : null}
              {tlStyle === "wave" ? (
                <svg viewBox="0 0 100 100" preserveAspectRatio="none" style={{ position: "absolute", inset: 0, width: "100%", height: "100%" }}>
                  <path d={buildWavePath(stats.history.map((point) => point.total_packets), 100, 100)} fill="var(--accent-soft)" stroke="var(--accent)" strokeWidth="1.2" />
                </svg>
              ) : null}
              {trackMarkers.map((marker) => (
                <div key={`${marker.timestamp_ns}-${marker.label}`} className="tl-mark" style={{ left: marker.left, background: marker.color ?? "var(--amber)" }} data-label={marker.label} />
              ))}
              <div className="tl-scrub" style={{ left: `${progressPct}%` }} />
            </button>

            <div className="tl-ticks">
              {buildTicks(session.duration_ns).map((tick) => (
                <span key={tick}>{tick}</span>
              ))}
            </div>
          </div>
        </div>

        <div className="col right">
          <div className="col-hd">{strings.player.packetInspector}</div>
          <div className="col-bd">
            <div className="panel">
              <div className="panel-hd">
                <h3>Packet @ {formatNs(effectivePacket?.t_record ?? 0)}</h3>
                <span className="grow" />
                <span className="chip mint">{effectivePacket?.writer ?? "—"}</span>
              </div>
              <div className="panel-bd inspector" style={{ padding: "10px 12px" }}>
                {[
                  ["channel", effectivePacket?.channel],
                  ["plugin", effectivePacket?.plugin],
                  ["seq", effectivePacket?.seq?.toString()],
                  ["t_record", formatNs(effectivePacket?.t_record ?? 0)],
                  ["t_replay", formatNs(effectivePacket?.t_replay ?? 0)],
                  ["size", formatBytes(effectivePacket?.size)],
                  ["topic", effectivePacket?.topic],
                  ["writer", effectivePacket?.writer],
                ].map(([key, value]) => (
                  <div className="row2" key={key}>
                    <span className="k">{key}</span>
                    <span className="v">{value ?? "—"}</span>
                  </div>
                ))}
              </div>
            </div>

            <div className="panel" style={{ marginTop: 10 }}>
              <div className="panel-hd"><h3>{strings.player.hexDump}</h3></div>
              <div className="panel-bd">
                <pre
                  style={{
                    margin: 0,
                    minHeight: 120,
                    overflow: "auto",
                    font: "11px/1.5 var(--mono)",
                    color: "var(--code-text)",
                    background: "var(--code-bg)",
                    padding: 12,
                    borderRadius: 6,
                    border: "1px solid var(--line)",
                  }}
                >
                  {effectivePacket?.hex || "--"}
                </pre>
              </div>
            </div>

            <div className="panel" style={{ marginTop: 10 }}>
              <div className="panel-hd">
                <h3>{strings.player.replayTargets}</h3>
                <span className="grow" />
                <button
                  type="button"
                  className="btn ghost"
                  onClick={() => {
                    const nextTargets = effectiveReplayTargets.map((target) => ({ ...target, enabled: true }));
                    setRuntimeReplayTargets(nextTargets);
                    void setReplayTargets(nextTargets);
                  }}
                >
                  {strings.player.targetEnableAll}
                </button>
              </div>
              <div className="panel-bd" style={{ padding: "10px" }}>
                {effectiveReplayTargets.map((target) => (
                  <button key={target.id} type="button" className="tree-item" style={{ width: "100%" }} onClick={() => void handleReplayTargetToggle(target)}>
                    <span className={`dot ${getReplayTargetTone(target)}`} />
                    <span>{target.name}</span>
                    <span className="badge">{target.endpoint ?? target.protocol}</span>
                    <span className={`chip ${getReplayTargetTone(target)}`}>{getReplayTargetLabel(target)}</span>
                  </button>
                ))}
              </div>
            </div>
          </div>
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
}: {
  label: string;
  value: string;
  unit?: string;
  delta: string;
}) {
  return (
    <div className="stat">
      <div className="k">{label}</div>
      <div className="v">
        {value}
        {unit ? <span className="u">{unit}</span> : null}
      </div>
      <div className="d">{delta}</div>
    </div>
  );
}

function buildWavePath(values: number[], width: number, height: number): string {
  if (values.length === 0) {
    return `M0 ${height / 2} L${width} ${height / 2}`;
  }
  const max = Math.max(...values, 1);
  const top = values
    .map((value, index) => {
      const x = values.length === 1 ? width / 2 : (index / (values.length - 1)) * width;
      const y = height / 2 - (value / max) * (height / 2 - 6);
      return `${index === 0 ? "M" : "L"}${x.toFixed(2)} ${y.toFixed(2)}`;
    })
    .join(" ");
  const bottom = values
    .map((value, index) => {
      const reverseIndex = values.length - 1 - index;
      const x = values.length === 1 ? width / 2 : (reverseIndex / (values.length - 1)) * width;
      const y = height / 2 + (values[reverseIndex]! / max) * (height / 2 - 6);
      return `L${x.toFixed(2)} ${y.toFixed(2)}`;
    })
    .join(" ");
  return `${top} ${bottom} Z`;
}

function buildTicks(durationNs: number): string[] {
  if (durationNs <= 0) {
    return ["00:00", "00:00", "00:00", "00:00", "00:00"];
  }
  return Array.from({ length: 5 }, (_, index) => formatNs((durationNs / 4) * index).replace(/^00:/, ""));
}

function getReplayTargetTone(target: ReplayTarget): "mint" | "gray" | "rose" {
  if (!target.enabled) {
    return "gray";
  }
  if (target.status === "error") {
    return "rose";
  }
  return target.status === "active" ? "mint" : "gray";
}

function getReplayTargetLabel(target: ReplayTarget): string {
  if (!target.enabled) {
    return "Off";
  }
  if (target.status === "error") {
    return "Error";
  }
  return target.status === "active" ? "Active" : "Idle";
}
