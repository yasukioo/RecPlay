import { useEffect, useState } from "react";

import { getSessionState, getStatsSnapshot, getWebSocketUrl } from "./api/client";
import { MenuBar } from "./components/MenuBar/MenuBar";
import { TransportBar } from "./components/TransportBar/TransportBar";
import { StatusBar } from "./components/StatusBar/StatusBar";
import { SessionInfo } from "./components/SessionInfo/SessionInfo";
import { Timeline } from "./components/Timeline/Timeline";
import { PluginTree } from "./components/PluginTree/PluginTree";
import { PluginCard } from "./components/PluginCard/PluginCard";
import { MonitorDashboard } from "./components/MonitorDashboard/MonitorDashboard";
import { RecordPanel } from "./components/RecordPanel/RecordPanel";
import { PlaybackPanel } from "./components/PlaybackPanel/PlaybackPanel";
import { useWebSocket } from "./hooks/useWebSocket";
import { useSessionStore } from "./stores/sessionStore";
import { useStatsStore } from "./stores/statsStore";
import { usePluginStore } from "./stores/pluginStore";

export default function App() {
  const [wsStatus, setWsStatus] = useState<"connecting" | "open" | "closed" | "error">("connecting");
  const [errorMessage, setErrorMessage] = useState<string | null>(null);

  const selectedPluginId = usePluginStore((s) => s.selectedPluginId);
  const plugins = usePluginStore((s) => s.plugins);
  const selectedPlugin = plugins.find((p) => p.id === selectedPluginId) ?? null;

  // Bootstrap initial state from REST
  useEffect(() => {
    Promise.all([getSessionState(), getStatsSnapshot()])
      .then(([session, stats]) => {
        useSessionStore.getState().replaceSnapshot(session);
        useStatsStore.getState().replaceSnapshot(stats);
      })
      .catch((err: unknown) => {
        setErrorMessage(err instanceof Error ? err.message : "Failed to load initial data");
      });
  }, []);

  // WebSocket realtime updates
  useWebSocket({
    url: getWebSocketUrl(),
    enabled: true,
    onStatusChange: setWsStatus,
  });

  return (
    <div className="h-screen flex flex-col bg-hmi-bg text-hmi-text">
      {/* Top: Menu Bar */}
      <MenuBar />

      {/* Error banner */}
      {errorMessage && (
        <div className="px-4 py-2 bg-hmi-danger/10 border-b border-hmi-danger/30 text-sm text-hmi-danger">
          {errorMessage}
          <button
            type="button"
            onClick={() => setErrorMessage(null)}
            className="ml-2 underline text-xs"
          >
            dismiss
          </button>
        </div>
      )}

      {/* Main three-column area */}
      <main className="flex-1 grid grid-cols-[220px_1fr_260px] gap-0 overflow-hidden">
        {/* Left: Plugin Tree */}
        <div className="border-r border-hmi-border overflow-y-auto p-2">
          <PluginTree />
        </div>

        {/* Center: Timeline + Dashboard + Panels */}
        <div className="overflow-y-auto p-3 space-y-3">
          <Timeline />
          <MonitorDashboard />

          <div className="grid grid-cols-2 gap-3">
            <RecordPanel onError={setErrorMessage} />
            <PlaybackPanel onError={setErrorMessage} />
          </div>

          {/* Plugin Card (shown when a plugin is selected) */}
          {selectedPluginId && <PluginCard plugin={selectedPlugin} />}
        </div>

        {/* Right: Session Info */}
        <div className="border-l border-hmi-border overflow-y-auto">
          <SessionInfo />
        </div>
      </main>

      {/* Bottom: Transport Bar */}
      <TransportBar />

      {/* Footer: Status Bar */}
      <StatusBar wsStatus={wsStatus} />
    </div>
  );
}
