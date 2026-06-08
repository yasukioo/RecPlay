import { useEffect, useState } from "react";

import {
  getChannels,
  getCurrentPacket,
  getEventLog,
  getMarkers,
  getPlugins,
  getReplayTargets,
  getSessionState,
  getStatsSnapshot,
  getWebSocketUrl,
  listRecordingFiles,
} from "./api/client";
import { useWebSocket } from "./hooks/useWebSocket";
import { useSessionStore } from "./stores/sessionStore";
import { useStatsStore } from "./stores/statsStore";
import { usePluginStore } from "./stores/pluginStore";
import { useRuntimeStore } from "./stores/runtimeStore";
import { AppShell } from "./views/AppShell";
import { buildChannelView, buildFallbackPacket } from "./views/workbenchModel";

/**
 * App entry for the DRR-style frontend. Keeps the plumbing alive — bootstraps
 * the Zustand stores from REST and pumps realtime updates over WebSocket — then
 * hands the chrome + workbenches to AppShell.
 *
 * CODEX (阶段 8R): the rich per-view handlers (plugin toggle/save, transport
 * commands, keyboard shortcuts) live in App.legacy.tsx — fold them into the new
 * RecorderView / PlayerView as those views gain real controls.
 */
export default function App() {
  const [wsStatus, setWsStatus] = useState<"connecting" | "open" | "closed" | "error">("connecting");

  useEffect(() => {
    Promise.all([
      getSessionState(),
      getStatsSnapshot(),
      getPlugins(),
      getChannels(),
      listRecordingFiles(),
      getMarkers(),
      getReplayTargets(),
      getCurrentPacket(),
      getEventLog(),
    ])
      .then(([session, stats, plugins, channels, files, markers, replayTargets, currentPacket, eventLog]) => {
        useSessionStore.getState().replaceSnapshot(session);
        useStatsStore.getState().replaceSnapshot(stats);
        usePluginStore.getState().setPlugins(plugins);
        const channelView = buildChannelView(channels, stats);
        useRuntimeStore.getState().setChannels(channelView);
        useRuntimeStore.getState().setRecordingFiles(files);
        useRuntimeStore.getState().setMarkers(markers);
        useRuntimeStore.getState().setReplayTargets(replayTargets);
        useRuntimeStore.getState().setCurrentPacket(
          currentPacket ??
            buildFallbackPacket(
              channelView[0] ?? null,
              session.position_ns,
              session.duration_ns,
            ),
        );
        useRuntimeStore.getState().replaceEventLog(eventLog);
      })
      .catch(() => {
        // Non-fatal during skeleton phase: the workbenches show placeholders.
        // CODEX: surface an error banner once views consume live data.
      });
  }, []);

  useWebSocket({
    url: getWebSocketUrl(),
    enabled: true,
    onStatusChange: setWsStatus,
  });

  return <AppShell wsStatus={wsStatus} />;
}
