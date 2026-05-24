import { lazy, Suspense, useCallback, useEffect, useRef, useState } from "react";

import {
  getChannels,
  getPluginDetail,
  getPlugins,
  getSessionState,
  getStatsSnapshot,
  getTopicMappings,
  getWebSocketUrl,
  pausePlayback,
  pauseRecording,
  playPlayback,
  resumeRecording,
  setPluginConfig,
  startRecording,
  startPlugin,
  stopPlugin,
  stopPlayback,
  stopRecording,
  setTopicMappings,
} from "./api/client";
import { MenuBar } from "./components/MenuBar/MenuBar";
import { TransportBar } from "./components/TransportBar/TransportBar";
import {
  resolveGlobalShortcut,
  resolveTransportCommand,
  type TransportCommand,
} from "./components/TransportBar/transportBarModel";
import { StatusBar } from "./components/StatusBar/StatusBar";
import { SessionInfo } from "./components/SessionInfo/SessionInfo";
import { TIMELINE_FOCUS_ID } from "./components/Timeline/timelineModel";
import { PluginTree } from "./components/PluginTree/PluginTree";
import { PluginCard } from "./components/PluginCard/PluginCard";
import { RecordPanel } from "./components/RecordPanel/RecordPanel";
import { PlaybackPanel } from "./components/PlaybackPanel/PlaybackPanel";
import { TopicMapper } from "./components/TopicMapper/TopicMapper";
import { useWebSocket } from "./hooks/useWebSocket";
import { getTranslations } from "./i18n";
import { getAppShellClasses, getPanelGridClasses } from "./layoutModel";
import { LOCALE_STORAGE_KEY, sanitizeLocale, useLocaleStore } from "./stores/localeStore";
import { THEME_STORAGE_KEY, sanitizeTheme, useThemeStore } from "./stores/themeStore";
import { useSessionStore } from "./stores/sessionStore";
import { useStatsStore } from "./stores/statsStore";
import { usePluginStore } from "./stores/pluginStore";
import type { Channel, Locale, TopicMapping } from "./types";

const DEFAULT_SHORTCUT_RECORDING_CONFIG = {
  output_path: "capture.rpcap",
  protocols: ["UDP", "TCP"],
};

const Timeline = lazy(async () => ({
  default: (await import("./components/Timeline/Timeline")).Timeline,
}));

const MonitorDashboard = lazy(async () => ({
  default: (await import("./components/MonitorDashboard/MonitorDashboard")).MonitorDashboard,
}));

export default function App() {
  const [wsStatus, setWsStatus] = useState<"connecting" | "open" | "closed" | "error">("connecting");
  const [errorMessage, setErrorMessage] = useState<string | null>(null);
  const [channels, setChannels] = useState<Channel[]>([]);
  const [mappings, setMappings] = useState<TopicMapping[]>([]);
  const shortcutBusyRef = useRef(false);

  const sessionState = useSessionStore((s) => s.state);
  const speed = useSessionStore((s) => s.speed);
  const locale = useLocaleStore((s) => s.locale);
  const setLocale = useLocaleStore((s) => s.setLocale);
  const toggleLocale = useLocaleStore((s) => s.toggleLocale);
  const theme = useThemeStore((s) => s.theme);
  const setTheme = useThemeStore((s) => s.setTheme);
  const toggleTheme = useThemeStore((s) => s.toggleTheme);
  const selectedPluginId = usePluginStore((s) => s.selectedPluginId);
  const plugins = usePluginStore((s) => s.plugins);
  const selectedPlugin = plugins.find((p) => p.id === selectedPluginId) ?? null;
  const t = getTranslations(locale);
  const shellClasses = getAppShellClasses();
  const panelGridClasses = getPanelGridClasses();

  // Bootstrap initial state from REST
  useEffect(() => {
    Promise.all([getSessionState(), getStatsSnapshot(), getPlugins(), getChannels(), getTopicMappings()])
      .then(([session, stats, pluginList, channelList, mappingList]) => {
        useSessionStore.getState().replaceSnapshot(session);
        useStatsStore.getState().replaceSnapshot(stats);
        usePluginStore.getState().setPlugins(pluginList);
        setChannels(channelList);
        setMappings(mappingList);
      })
      .catch((err: unknown) => {
        setErrorMessage(err instanceof Error ? err.message : t.errors.loadInitialData);
      });
  }, [t.errors.loadInitialData]);

  useEffect(() => {
    if (typeof window === "undefined") {
      return;
    }

    setLocale(sanitizeLocale(window.localStorage.getItem(LOCALE_STORAGE_KEY)));
  }, [setLocale]);

  useEffect(() => {
    if (typeof window === "undefined") {
      return;
    }

    setTheme(sanitizeTheme(window.localStorage.getItem(THEME_STORAGE_KEY)));
  }, [setTheme]);

  useEffect(() => {
    if (typeof document === "undefined") {
      return;
    }

    document.documentElement.lang = locale;
    document.documentElement.dataset.theme = theme;
    document.documentElement.style.colorScheme = theme;

    if (typeof window !== "undefined") {
      window.localStorage.setItem(LOCALE_STORAGE_KEY, locale);
      window.localStorage.setItem(THEME_STORAGE_KEY, theme);
    }
  }, [locale, theme]);

  // WebSocket realtime updates
  useWebSocket({
    url: getWebSocketUrl(),
    enabled: true,
    onStatusChange: setWsStatus,
  });

  const refreshChannels = useCallback(async () => {
    setChannels(await getChannels());
  }, []);

  const handleMappingChange = (nextMappings: TopicMapping[]) => {
    setMappings(nextMappings);
    setTopicMappings(nextMappings).catch((err: unknown) => {
      setErrorMessage(err instanceof Error ? err.message : t.errors.saveTopicMappings);
    });
  };

  const handlePluginToggle = useCallback(async (pluginId: string) => {
    const plugin = usePluginStore.getState().plugins.find((item) => item.id === pluginId);
    if (!plugin) {
      return;
    }

    try {
      setErrorMessage(null);
      const nextPlugin =
        plugin.state === "active" ? await stopPlugin(pluginId) : await startPlugin(pluginId);
      usePluginStore.getState().updatePlugin(pluginId, nextPlugin);
      await refreshChannels();
    } catch (err: unknown) {
      setErrorMessage(err instanceof Error ? err.message : t.errors.transportCommand);
    }
  }, [refreshChannels, t.errors.transportCommand]);

  const handlePluginSaveConfig = useCallback(async (
    pluginId: string,
    configFields: NonNullable<NonNullable<typeof selectedPlugin>["config_fields"]>,
  ) => {
    try {
      setErrorMessage(null);
      const nextPlugin = await setPluginConfig(pluginId, configFields);
      usePluginStore.getState().updatePlugin(pluginId, nextPlugin);

      if (selectedPluginId === pluginId) {
        const detail = await getPluginDetail(pluginId);
        usePluginStore.getState().updatePlugin(pluginId, detail);
      }
    } catch (err: unknown) {
      setErrorMessage(err instanceof Error ? err.message : t.errors.transportCommand);
    }
  }, [selectedPluginId, t.errors.transportCommand]);

  const executeShortcutCommand = useCallback(async (command: TransportCommand) => {
    if (shortcutBusyRef.current) {
      return;
    }

    const operations: Record<TransportCommand, () => Promise<unknown>> = {
      startRecording: () => startRecording(DEFAULT_SHORTCUT_RECORDING_CONFIG),
      pauseRecording,
      resumeRecording,
      playPlayback: () => playPlayback(speed),
      pausePlayback,
      stopRecording,
      stopPlayback,
    };

    try {
      shortcutBusyRef.current = true;
      setErrorMessage(null);
      await operations[command]();
    } catch (err: unknown) {
      setErrorMessage(err instanceof Error ? err.message : t.errors.keyboardShortcut);
    } finally {
      shortcutBusyRef.current = false;
    }
  }, [speed, t.errors.keyboardShortcut]);

  useEffect(() => {
    const handleKeyDown = (event: KeyboardEvent) => {
      const target = event.target instanceof HTMLElement ? event.target : null;
      const action = resolveGlobalShortcut({
        key: event.key,
        code: event.code,
        altKey: event.altKey,
        ctrlKey: event.ctrlKey,
        metaKey: event.metaKey,
        shiftKey: event.shiftKey,
        repeat: event.repeat,
        targetTagName: target?.tagName,
        targetIsContentEditable: target?.isContentEditable,
      }, sessionState);

      if (!action) {
        return;
      }

      event.preventDefault();

      if (action.type === "focusTimeline") {
        const timeline = document.getElementById(TIMELINE_FOCUS_ID);
        if (timeline instanceof HTMLElement) {
          timeline.focus();
        }
        return;
      }

      const command = resolveTransportCommand(action.control, sessionState);
      if (command) {
        void executeShortcutCommand(command);
      }
    };

    window.addEventListener("keydown", handleKeyDown);
    return () => window.removeEventListener("keydown", handleKeyDown);
  }, [executeShortcutCommand, sessionState]);

  const chartPanelFallback = (
    <div
      aria-hidden="true"
      className="min-h-24 rounded border border-hmi-border bg-hmi-surface"
    />
  );

  return (
    <div className={shellClasses.app}>
      <div className={shellClasses.root}>
      {/* Top: Menu Bar */}
      <MenuBar
        wsStatus={wsStatus}
        theme={theme}
        locale={locale}
        onToggleTheme={toggleTheme}
        onToggleLocale={toggleLocale}
      />

      {/* Error banner */}
      {errorMessage && (
        <div className="px-4 py-2 bg-hmi-danger/10 border-b border-hmi-danger/30 text-sm text-hmi-danger">
          {errorMessage}
          <button
            type="button"
            onClick={() => setErrorMessage(null)}
            className="ml-2 underline text-xs"
          >
            {t.buttons.dismiss}
          </button>
        </div>
      )}

      {/* Main three-column area */}
      <main className={shellClasses.main}>
        {/* Left: Plugin Tree */}
        <div className={shellClasses.leftRail}>
          <PluginTree onTogglePlugin={handlePluginToggle} />
        </div>

        {/* Center: Timeline + Dashboard + Panels */}
        <div className={shellClasses.center}>
          <Suspense fallback={chartPanelFallback}>
            <Timeline />
          </Suspense>
          <Suspense fallback={chartPanelFallback}>
            <MonitorDashboard />
          </Suspense>

          <div className={panelGridClasses}>
            <RecordPanel onError={setErrorMessage} />
            <PlaybackPanel onError={setErrorMessage} />
          </div>

          <TopicMapper
            channels={channels}
            mappings={mappings}
            onMappingChange={handleMappingChange}
          />

          {/* Plugin Card (shown when a plugin is selected) */}
          {selectedPluginId && (
            <PluginCard
              key={selectedPluginId}
              plugin={selectedPlugin}
              onTogglePlugin={handlePluginToggle}
              onSaveConfig={handlePluginSaveConfig}
            />
          )}
        </div>

        {/* Right: Session Info */}
        <div className={shellClasses.rightRail}>
          <SessionInfo />
        </div>
      </main>

      {/* Bottom: Transport Bar */}
      <TransportBar locale={locale} onError={setErrorMessage} />

      {/* Footer: Status Bar */}
      <StatusBar locale={locale} wsStatus={wsStatus} />
      </div>
    </div>
  );
}
