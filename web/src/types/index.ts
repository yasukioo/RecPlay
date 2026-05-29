/** Shared types for RecPlay frontend */

export type ThemeMode = "dark" | "light";
export type Locale = "en-US" | "zh-CN";

export type SessionState =
  | "Idle"
  | "Recording"
  | "RecordingPaused"
  | "Playing"
  | "PlaybackPaused"
  | "PlayingPaused"
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
  drop_rate: number;
  write_latency_p99_ms: number;
  ringbuf_used: number;
  ringbuf_capacity: number;
  disk_queue_bytes: number;
  cpu_usage_percent: number | null;
}

export interface StatsHistoryPoint {
  timestamp: number;
  total_packets: number;
  total_drops: number;
  total_throughput_mbps: number;
  drop_rate?: number;
  write_latency_p99_ms?: number;
  ringbuf_used?: number;
  ringbuf_capacity?: number;
  disk_queue_bytes?: number;
  cpu_usage_percent?: number | null;
}

export interface PluginInfo {
  id: string;
  name: string;
  version: string;
  state: "active" | "inactive" | "error";
  bundle_path: string;
  config_fields?: Array<{
    key: string;
    label: string;
    value: string;
  }>;
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
}

export interface Channel {
  id: string;
  topic: string;
  direction: "input" | "output";
  protocol: string;
  plugin_id: string;
}

export interface TopicMapping {
  source_topic: string;
  target_topic: string;
  transform?: string;
}
