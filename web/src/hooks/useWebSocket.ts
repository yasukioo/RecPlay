import { useEffect, useRef } from "react";
import { useSessionStore } from "../stores/sessionStore";
import { useStatsStore } from "../stores/statsStore";
import { usePluginStore } from "../stores/pluginStore";
import { useRuntimeStore } from "../stores/runtimeStore";
import type { LogEntry, PacketInspect, PluginInfo, ReplayTarget, SessionTransition, StatsSnapshot, TimelineMarker } from "../types";

export type RealtimeMessage =
  | { type: "stats"; data: Partial<StatsSnapshot> }
  | { type: "state_changed"; data: SessionTransition }
  | { type: "timeline"; data: { position_ns: number; duration_ns: number } }
  | { type: "plugin_event"; data: { plugin_id: string; patch: Partial<PluginInfo>; message?: string } }
  | { type: "event_log"; data: LogEntry }
  | { type: "packet_inspect"; data: PacketInspect | null }
  | { type: "markers"; data: TimelineMarker[] }
  | { type: "replay_targets"; data: ReplayTarget[] };

export function parseRealtimeMessage(raw: string): RealtimeMessage | null {
  let parsed: { type?: string; data?: unknown };
  try {
    parsed = JSON.parse(raw) as { type?: string; data?: unknown };
  } catch {
    return null;
  }
  if (!parsed.type || !parsed.data || typeof parsed.data !== "object") return null;

  switch (parsed.type) {
    case "stats":
      return { type: "stats", data: parsed.data as Partial<StatsSnapshot> };
    case "state_changed":
      return { type: "state_changed", data: parsed.data as SessionTransition };
    case "timeline":
      return { type: "timeline", data: parsed.data as { position_ns: number; duration_ns: number } };
    case "plugin_event":
      return { type: "plugin_event", data: parsed.data as { plugin_id: string; patch: Partial<PluginInfo>; message?: string } };
    case "event_log":
      return { type: "event_log", data: parsed.data as LogEntry };
    case "packet_inspect":
      return { type: "packet_inspect", data: parsed.data as PacketInspect | null };
    case "markers":
      return { type: "markers", data: parsed.data as TimelineMarker[] };
    case "replay_targets":
      return { type: "replay_targets", data: parsed.data as ReplayTarget[] };
    default:
      return null;
  }
}

function applyRealtimeMessage(message: RealtimeMessage): void {
  switch (message.type) {
    case "stats":
      useStatsStore.getState().replaceSnapshot(message.data);
      break;
    case "state_changed":
      useSessionStore.getState().applyTransition(message.data);
      break;
    case "timeline":
      // Update session position from timeline tick
      useSessionStore.setState({
        position_ns: message.data.position_ns,
        duration_ns: message.data.duration_ns,
      });
      break;
    case "plugin_event":
      usePluginStore.getState().updatePlugin(message.data.plugin_id, message.data.patch);
      if (message.data.message) {
        useRuntimeStore.getState().appendEventLog({
          id: `${Date.now()}-${message.data.plugin_id}`,
          ts: Date.now(),
          level: "info",
          source: message.data.plugin_id,
          message: message.data.message,
        });
      }
      break;
    case "event_log":
      useRuntimeStore.getState().appendEventLog(message.data);
      break;
    case "packet_inspect":
      useRuntimeStore.getState().setCurrentPacket(message.data);
      break;
    case "markers":
      useRuntimeStore.getState().setMarkers(message.data);
      break;
    case "replay_targets":
      useRuntimeStore.getState().setReplayTargets(message.data);
      break;
  }
}

export interface UseWebSocketOptions {
  url: string;
  enabled?: boolean;
  onStatusChange?: (status: "connecting" | "open" | "closed" | "error") => void;
}

const BASE_RECONNECT_MS = 1000;
const MAX_RECONNECT_MS = 30000;

export function useWebSocket({ url, enabled = true, onStatusChange }: UseWebSocketOptions): void {
  const reconnectTimerRef = useRef<number | null>(null);
  const attemptRef = useRef(0);

  useEffect(() => {
    if (!enabled) return undefined;

    let cancelled = false;
    let socket: WebSocket | null = null;

    const clearReconnect = () => {
      if (reconnectTimerRef.current !== null) {
        window.clearTimeout(reconnectTimerRef.current);
        reconnectTimerRef.current = null;
      }
    };

    const getBackoff = () => {
      const delay = Math.min(BASE_RECONNECT_MS * 2 ** attemptRef.current, MAX_RECONNECT_MS);
      attemptRef.current++;
      return delay;
    };

    const connect = () => {
      clearReconnect();
      onStatusChange?.("connecting");
      socket = new WebSocket(url);

      socket.addEventListener("open", () => {
        attemptRef.current = 0;
        onStatusChange?.("open");
      });

      socket.addEventListener("message", (event) => {
        const message = parseRealtimeMessage(String(event.data));
        if (message) applyRealtimeMessage(message);
      });

      socket.addEventListener("error", () => {
        onStatusChange?.("error");
      });

      socket.addEventListener("close", () => {
        onStatusChange?.("closed");
        if (!cancelled) {
          reconnectTimerRef.current = window.setTimeout(connect, getBackoff());
        }
      });
    };

    connect();

    return () => {
      cancelled = true;
      clearReconnect();
      if (socket) socket.close();
    };
  }, [enabled, onStatusChange, url]);
}
