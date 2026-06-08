/** Shared types for RecPlay frontend */

export type ThemeMode = "dark" | "light";
export type Locale = "en-US" | "zh-CN";
export type PluginKind = "Source" | "Sink" | "Codec" | "Service";
export type LogLevel = "ok" | "info" | "warn" | "err";

export type SessionState =
  | "Idle"
  | "Recording"
  | "RecordingPaused"
  | "Playing"
  | "PlaybackPaused"
  | "Seeking"
  | "Stopped"
  | "Unknown";

export interface SessionTransition {
  old_state: string;
  new_state: string;
}

export interface SessionConfig {
  recordPath: string;
  playbackPath: string;
}

export interface SessionStateSnapshot {
  state: SessionState;
  duration_ns: number;
  position_ns: number;
  speed: number;
  loop_start_ns: number | null;
  loop_end_ns: number | null;
  lastTransition: SessionTransition | null;
  config: SessionConfig;
}

export interface StatsSnapshot {
  total_throughput_mbps: number;
  total_packets: number;
  total_drops: number;
  total_bytes: number;
  drop_rate: number;
  write_latency_p99_ms: number;
  ringbuf_used: number;
  ringbuf_capacity: number;
  disk_queue_bytes: number;
  cpu_usage_percent: number | null;
  per_protocol?: Record<string, ProtocolStats>;
  per_channel?: Record<string, ChannelStats>;
}

export interface StatsHistoryPoint {
  timestamp: number;
  total_packets: number;
  total_drops: number;
  total_bytes: number;
  total_throughput_mbps: number;
  drop_rate?: number;
  write_latency_p99_ms?: number;
  ringbuf_used?: number;
  ringbuf_capacity?: number;
  disk_queue_bytes?: number;
  cpu_usage_percent?: number | null;
  per_protocol?: Record<string, ProtocolStats>;
  per_channel?: Record<string, ChannelStats>;
}

export interface ProtocolStats {
  throughput_mbps: number;
  packets: number;
  drops?: number;
}

export interface ChannelStats {
  throughput_mbps: number;
  packets: number;
  drops?: number;
}

export interface PluginConfigField {
  key: string;
  label: string;
  value: string;
}

export interface PluginInfo {
  id: string;
  name: string;
  version: string;
  state: "active" | "inactive" | "error";
  bundle_path: string;
  kind?: PluginKind;
  protocol?: string;
  desc?: string;
  config?: Record<string, string>;
  config_fields?: PluginConfigField[];
}

export interface TimelineState {
  position_ns: number;
  duration_ns: number;
  markers: TimelineMarker[];
}

export interface TimelineMarker {
  timestamp_ns: number;
  label: string;
  type: "event" | "error" | "bookmark";
  color?: string;
}

export interface Channel {
  id: string;
  name?: string;
  topic: string;
  direction: "input" | "output";
  protocol: string;
  plugin_id: string;
  rate?: number;
  bytes?: number;
  drop?: number;
  color?: string;
  enabled?: boolean;
}

export interface TopicMapping {
  source_topic: string;
  target_topic: string;
  transform?: string;
}

export interface RecordingFile {
  name: string;
  path?: string | null;
  size: number | null;
  duration_ns: number | null;
  channels: number | null;
  recorded_at: number | null;
  tag: string | null;
  codec?: string | null;
  checksum?: string | null;
  mock?: boolean;
}

export interface PacketInspect {
  channel: string;
  plugin: string;
  seq: number;
  t_record: number;
  t_replay: number;
  size: number;
  topic: string;
  writer: string;
  hex: string;
  metadata?: Record<string, string>;
  mock?: boolean;
}

export interface ReplayTarget {
  id: string;
  name: string;
  protocol: string;
  enabled: boolean;
  endpoint?: string;
  status?: "active" | "idle" | "error";
  mock?: boolean;
}

export interface LogEntry {
  id: string;
  ts: number;
  level: LogLevel;
  message: string;
  source?: string;
  mock?: boolean;
}

/** Event-density of the open playback file: total packet counts per time bucket. */
export interface DensityResponse {
  duration_ns: number;
  buckets: number;
  total: number[];
}
