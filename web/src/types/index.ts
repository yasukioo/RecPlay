/** Shared types for RecPlay frontend */

export type SessionState =
  | "Idle"
  | "Recording"
  | "RecordingPaused"
  | "Playing"
  | "PlayingPaused"
  | "Unknown";

export interface SessionTransition {
  old_state: string;
  new_state: string;
}

export interface SessionStateSnapshot {
  state: SessionState;
  duration_ns: number;
  position_ns: number;
  speed: number;
  lastTransition: SessionTransition | null;
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
}

export interface StatsHistoryPoint {
  timestamp: number;
  total_packets: number;
  total_drops: number;
  total_throughput_mbps: number;
}

export interface PluginInfo {
  id: string;
  name: string;
  version: string;
  state: "active" | "inactive" | "error";
  bundle_path: string;
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
