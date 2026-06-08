import type {
  Channel,
  ChannelStats,
  LogEntry,
  PacketInspect,
  PluginInfo,
  PluginKind,
  RecordingFile,
  ReplayTarget,
  StatsHistoryPoint,
  StatsSnapshot,
  TimelineMarker,
} from "../types";

const CHANNEL_COLORS = ["var(--accent)", "var(--mint)", "var(--amber)", "var(--violet)", "var(--rose)"];
const REPLAY_PROTOCOLS = ["UDP", "TCP", "DDS"] as const;

export function inferPluginKind(plugin: Pick<PluginInfo, "id" | "name">): PluginKind {
  const token = `${plugin.id} ${plugin.name}`.toLowerCase();
  if (/(source|reader|capture|input)/.test(token)) {
    return "Source";
  }
  if (/(sink|writer|output|store|playback)/.test(token)) {
    return "Sink";
  }
  if (/(codec|compress|decode|encode)/.test(token)) {
    return "Codec";
  }
  return "Service";
}

export function inferProtocol(plugin: Pick<PluginInfo, "id" | "name">): string | undefined {
  const token = `${plugin.id} ${plugin.name}`.toUpperCase();
  if (token.includes("UDP")) {
    return "UDP";
  }
  if (token.includes("TCP")) {
    return "TCP";
  }
  if (token.includes("DDS")) {
    return "DDS";
  }
  if (token.includes("SERIAL")) {
    return "Serial";
  }
  return undefined;
}

export function buildPluginConfigMap(plugin: PluginInfo): Record<string, string> {
  return Object.fromEntries((plugin.config_fields ?? []).map((field) => [field.key, field.value]));
}

export function getChannelNumericId(channelId: string): string {
  const parts = channelId.split(":");
  return parts[parts.length - 1] ?? channelId;
}

export function getChannelColor(index: number): string {
  return CHANNEL_COLORS[index % CHANNEL_COLORS.length] ?? "var(--accent)";
}

export function formatBytes(value: number | null | undefined): string {
  if (typeof value !== "number" || !Number.isFinite(value) || value < 0) {
    return "—";
  }
  if (value < 1024) {
    return `${Math.round(value)} B`;
  }
  if (value < 1024 * 1024) {
    return `${(value / 1024).toFixed(1)} KiB`;
  }
  if (value < 1024 * 1024 * 1024) {
    return `${(value / (1024 * 1024)).toFixed(1)} MiB`;
  }
  return `${(value / (1024 * 1024 * 1024)).toFixed(2)} GiB`;
}

export function formatRate(value: number | null | undefined, fractionDigits = 1): string {
  if (typeof value !== "number" || !Number.isFinite(value)) {
    return "—";
  }
  return value.toFixed(fractionDigits);
}

export function formatPercent(value: number | null | undefined): string {
  if (typeof value !== "number" || !Number.isFinite(value)) {
    return "—";
  }
  return `${Math.round(value)}%`;
}

export function formatNs(value: number | null | undefined): string {
  if (typeof value !== "number" || !Number.isFinite(value) || value < 0) {
    return "—";
  }
  const totalMs = value / 1_000_000;
  const hours = Math.floor(totalMs / 3_600_000);
  const minutes = Math.floor((totalMs % 3_600_000) / 60_000);
  const seconds = Math.floor((totalMs % 60_000) / 1000);
  const millis = Math.floor(totalMs % 1000);
  if (hours > 0) {
    return `${String(hours).padStart(2, "0")}:${String(minutes).padStart(2, "0")}:${String(seconds).padStart(2, "0")}.${String(millis).padStart(3, "0")}`;
  }
  return `${String(minutes).padStart(2, "0")}:${String(seconds).padStart(2, "0")}.${String(millis).padStart(3, "0")}`;
}

export function formatShortDuration(value: number | null | undefined): string {
  if (typeof value !== "number" || !Number.isFinite(value) || value < 0) {
    return "—";
  }
  const seconds = value / 1_000_000_000;
  if (seconds < 60) {
    return `${seconds.toFixed(1)} s`;
  }
  const minutes = Math.floor(seconds / 60);
  const remainder = seconds % 60;
  return `${minutes}m ${remainder.toFixed(0)}s`;
}

export function formatClock(date: Date): string {
  const hours = String(date.getHours()).padStart(2, "0");
  const minutes = String(date.getMinutes()).padStart(2, "0");
  const seconds = String(date.getSeconds()).padStart(2, "0");
  return `${hours}:${minutes}:${seconds}`;
}

export function buildChannelView(
  channels: Channel[],
  snapshot: StatsSnapshot,
): Channel[] {
  return channels.map((channel, index) => {
    const stats = resolveChannelStats(channel, snapshot);
    return {
      ...channel,
      color: channel.color ?? getChannelColor(index),
      rate: channel.rate ?? stats?.throughput_mbps ?? 0,
      bytes: channel.bytes ?? stats?.packets ?? 0,
      drop: channel.drop ?? stats?.drops ?? 0,
      enabled: channel.enabled !== false,
      name: channel.name ?? getLastSegment(channel.topic, channel.id),
    };
  });
}

export function buildSparklineSeries(history: StatsHistoryPoint[], channel: Channel): number[] {
  const channelKey = getChannelNumericId(channel.id);
  return history.map((point) => point.per_channel?.[channelKey]?.throughput_mbps ?? 0);
}

export function buildTimelineMarkers(
  markers: TimelineMarker[],
  history: StatsHistoryPoint[],
  durationNs: number,
): TimelineMarker[] {
  if (markers.length > 0) {
    return markers;
  }
  if (history.length < 2 || durationNs <= 0) {
    return [];
  }

  const lastIndex = history.length - 1;
  return history.flatMap((point, index) => {
    if (index === 0) {
      return [];
    }
    const previous = history[index - 1]!;
    const deltaDrops = point.total_drops - previous.total_drops;
    if (deltaDrops <= 0) {
      return [];
    }
    return [{
      timestamp_ns: Math.round(durationNs * (index / lastIndex)),
      label: `+${deltaDrops}`,
      type: "error" as const,
      color: "var(--rose)",
    }];
  });
}

export function buildReplayTargets(channels: Channel[], targets: ReplayTarget[]): ReplayTarget[] {
  if (targets.length > 0) {
    return targets;
  }
  return REPLAY_PROTOCOLS.map((protocol) => {
    const matched = channels.find((channel) => channel.protocol.toUpperCase() === protocol);
    return {
      id: protocol.toLowerCase(),
      name: `${protocol} Sink`,
      protocol,
      enabled: Boolean(matched),
      endpoint: matched?.topic,
      status: matched ? "active" : "idle",
      mock: true,
    };
  });
}

export function buildFallbackPacket(
  channel: Channel | null,
  positionNs: number,
  durationNs: number,
): PacketInspect | null {
  if (!channel) {
    return null;
  }
  const seq = Math.max(1, Math.round((positionNs / Math.max(durationNs, 1)) * 1024));
  return {
    channel: channel.id,
    plugin: channel.plugin_id,
    seq,
    t_record: positionNs,
    t_replay: positionNs,
    size: 0,
    topic: channel.topic,
    writer: channel.protocol,
    hex: "",
    mock: true,
  };
}

export function buildFallbackLogEntry(message: string, source?: string): LogEntry {
  return {
    id: `${Date.now()}-${Math.random().toString(16).slice(2, 8)}`,
    ts: Date.now(),
    level: "info",
    message,
    source,
    mock: true,
  };
}

export function getRecordingSessionName(recordPath: string, playbackPath: string, fallback: string): string {
  const path = playbackPath || recordPath;
  if (!path) {
    return fallback;
  }
  const normalized = path.replace(/\\/g, "/");
  return getLastSegment(normalized, fallback);
}

export function formatLogTimestamp(ts: number): string {
  return formatClock(new Date(ts));
}

export function formatFileMeta(file: RecordingFile | null): Array<{ key: string; value: string }> {
  if (!file) {
    return [
      { key: "name", value: "—" },
      { key: "duration", value: "—" },
      { key: "size", value: "—" },
      { key: "channels", value: "—" },
      { key: "recorded", value: "—" },
      { key: "tag", value: "—" },
      { key: "codec", value: "—" },
      { key: "checksum", value: "—" },
    ];
  }

  return [
    { key: "name", value: file.name },
    { key: "duration", value: formatShortDuration(file.duration_ns) },
    { key: "size", value: formatBytes(file.size) },
    { key: "channels", value: file.channels?.toString() ?? "—" },
    { key: "recorded", value: file.recorded_at ? new Date(file.recorded_at).toLocaleString() : "—" },
    { key: "tag", value: file.tag ?? "—" },
    { key: "codec", value: file.codec ?? "—" },
    { key: "checksum", value: file.checksum ?? "—" },
  ];
}

function resolveChannelStats(channel: Channel, snapshot: StatsSnapshot): ChannelStats | undefined {
  const key = getChannelNumericId(channel.id);
  return snapshot.per_channel?.[key];
}

function getLastSegment(path: string, fallback: string): string {
  const parts = path.split("/").filter(Boolean);
  return parts[parts.length - 1] ?? fallback;
}
