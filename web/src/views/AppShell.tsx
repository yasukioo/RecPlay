import { lazy, Suspense, useEffect, useMemo, useState } from "react";

import { getPlugins, setReplayTargets } from "../api/client";
import { useLocaleStore } from "../stores/localeStore";
import { usePluginStore } from "../stores/pluginStore";
import { useRuntimeStore } from "../stores/runtimeStore";
import { useSessionStore } from "../stores/sessionStore";
import { useStatsStore } from "../stores/statsStore";
import { useThemeStore } from "../stores/themeStore";
import {
  useUiStore,
  type AccentColor,
  type Density,
  type PluginLayout,
  type RateUi,
  type TimelineStyle,
} from "../stores/uiStore";
import type { Locale } from "../types";
import {
  formatBytes,
  formatClock,
  formatNs,
  formatPercent,
  formatRate,
  getRecordingSessionName,
} from "./workbenchModel";
import { getViewStrings } from "./viewStrings";
import { RecorderView } from "./RecorderView";

const PlayerView = lazy(async () => ({
  default: (await import("./PlayerView")).PlayerView,
}));

type WsStatus = "connecting" | "open" | "closed" | "error";

interface AppShellProps {
  wsStatus: WsStatus;
}

const ACCENT_SOFT: Record<AccentColor, string> = {
  "#3a7bff": "rgba(58,123,255,.18)",
  "#5eead4": "rgba(94,234,212,.18)",
  "#b794f6": "rgba(183,148,246,.18)",
  "#ff5e7e": "rgba(255,94,126,.18)",
};

const ACCENT_LABELS: Record<AccentColor, string> = {
  "#3a7bff": "Blue",
  "#5eead4": "Mint",
  "#b794f6": "Violet",
  "#ff5e7e": "Rose",
};

export function AppShell({ wsStatus }: AppShellProps) {
  const mode = useUiStore((s) => s.mode);
  const setMode = useUiStore((s) => s.setMode);
  const density = useUiStore((s) => s.density);
  const accent = useUiStore((s) => s.accent);
  const timelineStyle = useUiStore((s) => s.timelineStyle);
  const rateUi = useUiStore((s) => s.rateUi);
  const pluginLayout = useUiStore((s) => s.pluginLayout);
  const theme = useThemeStore((s) => s.theme);
  const setTheme = useThemeStore((s) => s.setTheme);
  const locale = useLocaleStore((s) => s.locale);
  const toggleLocale = useLocaleStore((s) => s.toggleLocale);
  const strings = getViewStrings(locale);
  const [settingsOpen, setSettingsOpen] = useState(false);

  useEffect(() => {
    if (typeof document === "undefined") {
      return;
    }

    const root = document.documentElement;
    root.lang = locale;
    root.dataset.theme = theme;
    root.style.colorScheme = theme;
    root.style.setProperty("--accent", accent);
    root.style.setProperty("--accent-soft", ACCENT_SOFT[accent]);
    document.body.dataset.density = density === "regular" ? "" : density;
  }, [accent, density, locale, theme]);

  useEffect(() => {
    const onKeyDown = (event: KeyboardEvent) => {
      const target = event.target as HTMLElement | null;
      if (target && /^(INPUT|TEXTAREA|SELECT)$/.test(target.tagName)) {
        return;
      }
      if (target?.isContentEditable) {
        return;
      }

      const key = event.key.toLowerCase();
      if (key === "r") {
        setMode("recorder");
      } else if (key === "p") {
        setMode("player");
      }
    };

    window.addEventListener("keydown", onKeyDown);
    return () => window.removeEventListener("keydown", onKeyDown);
  }, [setMode]);

  const dataMode = mode === "recorder" ? "record" : "play";

  return (
    <div className="app" data-mode={dataMode}>
      <Titlebar
        mode={mode}
        locale={locale}
        theme={theme}
        onSetMode={setMode}
        onSetTheme={setTheme}
        onToggleLocale={toggleLocale}
        onOpenSettings={() => setSettingsOpen(true)}
      />
      <Menubar />

      {mode === "recorder" ? (
        <RecorderView />
      ) : (
        <Suspense fallback={<ViewFallback />}>
          <PlayerView />
        </Suspense>
      )}

      <Statusbar wsStatus={wsStatus} mode={mode} locale={locale} />

      {settingsOpen && (
        <SettingsPanel
          locale={locale}
          theme={theme}
          density={density}
          accent={accent}
          timelineStyle={timelineStyle}
          rateUi={rateUi}
          pluginLayout={pluginLayout}
          onClose={() => setSettingsOpen(false)}
        />
      )}
    </div>
  );
}

function ViewFallback() {
  return (
    <div style={{ flex: 1, display: "flex", alignItems: "center", justifyContent: "center", color: "var(--text-mute)" }}>
      Loading…
    </div>
  );
}

function Titlebar({
  mode,
  locale,
  theme,
  onSetMode,
  onSetTheme,
  onToggleLocale,
  onOpenSettings,
}: {
  mode: string;
  locale: Locale;
  theme: string;
  onSetMode: (mode: "recorder" | "player") => void;
  onSetTheme: (theme: "dark" | "light") => void;
  onToggleLocale: () => void;
  onOpenSettings: () => void;
}) {
  const session = useSessionStore((s) => s.config);
  const strings = getViewStrings(locale);
  const sessionName = getRecordingSessionName(session.recordPath, session.playbackPath, "—");

  return (
    <div className="titlebar">
      <div className="win-btns">
        <i className="r" />
        <i className="y" />
        <i className="g" />
      </div>
      <div className="logo">
        <span className="dot" />
        <span>RecPlay</span>
        <span style={{ color: "var(--text-dim)", fontWeight: 400, letterSpacing: 0 }}>Data Record &amp; Replay</span>
      </div>
      <span className="ver">v1.0.0</span>
      <div className="crumb">
        {strings.common.session} · <b>{sessionName}</b>
      </div>

      <div className="spacer" />

      <div className="mode-switch" role="tablist" aria-label="Mode">
        <button type="button" className={"ms-btn rec " + (mode === "recorder" ? "on" : "")} onClick={() => onSetMode("recorder")}>
          <span className="ms-dot" />
          <span>● {strings.common.record.toUpperCase()}</span>
          <span className="ms-key">R</span>
        </button>
        <button type="button" className={"ms-btn play " + (mode === "player" ? "on" : "")} onClick={() => onSetMode("player")}>
          <span className="ms-dot" />
          <span>▶ {strings.common.replay.toUpperCase()}</span>
          <span className="ms-key">P</span>
        </button>
      </div>

      <div className="spacer" />

      <button type="button" className="btn ghost" onClick={onToggleLocale} title={strings.common.language}>
        {locale === "zh-CN" ? "EN" : "中文"}
      </button>
      <div className="theme-toggle" role="group" aria-label={strings.common.settings}>
        <button type="button" className={"tt-btn " + (theme !== "light" ? "on" : "")} onClick={() => onSetTheme("dark")} title={strings.common.dark}>
          ☾
        </button>
        <button type="button" className={"tt-btn " + (theme === "light" ? "on" : "")} onClick={() => onSetTheme("light")} title={strings.common.light}>
          ☀
        </button>
      </div>
      <button type="button" className="btn ghost icon" title={strings.common.search}>
        ⌕
      </button>
      <button type="button" className="btn ghost icon" title={strings.common.settings} onClick={onOpenSettings}>
        ⚙
      </button>
    </div>
  );
}

function Menubar() {
  const items = ["File", "Edit", "Session", "Plugins", "View", "Tools", "Help"];
  return (
    <div className="menubar">
      {items.map((label) => (
        <span key={label}>{label}</span>
      ))}
      <div style={{ flex: 1 }} />
      <span style={{ color: "var(--text-mute)", font: "10.5px/1 var(--mono)" }}>
        recplay-core 1.0.0 · cppmicroservices 3.7 · format .rpcap v1.0
      </span>
    </div>
  );
}

function Statusbar({ wsStatus, mode, locale }: { wsStatus: WsStatus; mode: string; locale: Locale }) {
  const snapshot = useStatsStore((s) => s);
  const plugins = usePluginStore((s) => s.plugins);
  const channels = useRuntimeStore((s) => s.channels);
  const strings = getViewStrings(locale);
  const activePlugins = plugins.filter((plugin) => plugin.state === "active").length;
  const channelRate = channels.reduce((sum, channel) => sum + (channel.rate ?? 0), 0);
  const state = useSessionStore((s) => s.state);

  const indicatorClass =
    mode === "recorder" || state === "Recording" || state === "RecordingPaused"
      ? "sb-dot sb-rec"
      : wsStatus === "connecting"
        ? "sb-dot sb-warn"
        : "sb-dot";

  return (
    <div className="statusbar">
      <span className={indicatorClass} />
      <span>{mode === "recorder" ? "REC" : "READY"}</span>
      <span>WS {wsStatus}</span>
      <span>CPU {formatPercent(snapshot.cpu_usage_percent)}</span>
      <span>RSS {strings.common.unavailable}</span>
      <span>Disk {formatBytes(null)}</span>
      <span>Plugins {activePlugins}/{plugins.length}</span>
      <span>Channels {channels.length} · {formatRate(channelRate, 0)} pkt/s</span>
      <div className="grow" />
      <span>Drift {formatRate(snapshot.write_latency_p99_ms, 1)} ms</span>
      <span style={{ color: "var(--text)" }}>{formatClock(new Date())}</span>
    </div>
  );
}

function SettingsPanel({
  locale,
  theme,
  density,
  accent,
  timelineStyle,
  rateUi,
  pluginLayout,
  onClose,
}: {
  locale: Locale;
  theme: "dark" | "light";
  density: Density;
  accent: AccentColor;
  timelineStyle: TimelineStyle;
  rateUi: RateUi;
  pluginLayout: PluginLayout;
  onClose: () => void;
}) {
  const strings = getViewStrings(locale);
  const setTheme = useThemeStore((s) => s.setTheme);
  const setDensity = useUiStore((s) => s.setDensity);
  const setAccent = useUiStore((s) => s.setAccent);
  const setTimelineStyle = useUiStore((s) => s.setTimelineStyle);
  const setRateUi = useUiStore((s) => s.setRateUi);
  const setPluginLayout = useUiStore((s) => s.setPluginLayout);
  const replayTargets = useRuntimeStore((s) => s.replayTargets);

  const settingsRows = useMemo(
    () => [
      {
        label: strings.settings.theme,
        content: (
          <div className="tl-rate">
            {(["dark", "light"] as const).map((value) => (
              <button key={value} type="button" className={theme === value ? "on" : ""} onClick={() => setTheme(value)}>
                {value === "dark" ? strings.common.dark : strings.common.light}
              </button>
            ))}
          </div>
        ),
      },
      {
        label: strings.settings.density,
        content: (
          <div className="tl-rate">
            {([
              ["compact", strings.settings.compact],
              ["regular", strings.settings.regular],
              ["comfortable", strings.settings.comfortable],
            ] as const).map(([value, label]) => (
              <button key={value} type="button" className={density === value ? "on" : ""} onClick={() => setDensity(value)}>
                {label}
              </button>
            ))}
          </div>
        ),
      },
      {
        label: strings.settings.accent,
        content: (
          <div style={{ display: "flex", gap: 8, flexWrap: "wrap" }}>
            {(Object.keys(ACCENT_LABELS) as AccentColor[]).map((value) => (
              <button
                key={value}
                type="button"
                className="btn"
                style={{
                  borderColor: accent === value ? value : "var(--line)",
                  boxShadow: accent === value ? `0 0 0 1px ${value} inset` : undefined,
                }}
                onClick={() => setAccent(value)}
              >
                <span className="dot" style={{ background: value }} />
                {ACCENT_LABELS[value]}
              </button>
            ))}
          </div>
        ),
      },
      {
        label: strings.settings.timelineStyle,
        content: (
          <div className="tl-rate">
            {([
              ["compact", strings.settings.compact],
              ["detail", "Detail"],
              ["wave", "Wave"],
            ] as const).map(([value, label]) => (
              <button key={value} type="button" className={timelineStyle === value ? "on" : ""} onClick={() => setTimelineStyle(value)}>
                {label}
              </button>
            ))}
          </div>
        ),
      },
      {
        label: strings.settings.rateControl,
        content: (
          <div className="tl-rate">
            {([
              ["buttons", strings.settings.buttons],
              ["slider", strings.settings.slider],
              ["knob", strings.settings.knob],
            ] as const).map(([value, label]) => (
              <button key={value} type="button" className={rateUi === value ? "on" : ""} onClick={() => setRateUi(value)}>
                {label}
              </button>
            ))}
          </div>
        ),
      },
      {
        label: strings.settings.pluginLayout,
        content: (
          <div className="tl-rate">
            {([
              ["list", strings.settings.list],
              ["grid", strings.settings.grid],
            ] as const).map(([value, label]) => (
              <button key={value} type="button" className={pluginLayout === value ? "on" : ""} onClick={() => setPluginLayout(value)}>
                {label}
              </button>
            ))}
          </div>
        ),
      },
    ],
    [
      accent,
      density,
      pluginLayout,
      rateUi,
      setAccent,
      setDensity,
      setPluginLayout,
      setRateUi,
      setTheme,
      setTimelineStyle,
      strings,
      theme,
      timelineStyle,
    ],
  );

  return (
    <div
      style={{
        position: "fixed",
        inset: 0,
        background: "rgba(0,0,0,.35)",
        display: "flex",
        alignItems: "center",
        justifyContent: "center",
        padding: 24,
        zIndex: 50,
      }}
      onMouseDown={(event) => {
        if (event.target === event.currentTarget) {
          onClose();
        }
      }}
    >
      <div className="panel" style={{ width: "min(780px, 100%)" }}>
        <div className="panel-hd">
          <h3>{strings.common.settings}</h3>
          <span className="grow" />
          <span className="chip gray">{replayTargets.length} targets</span>
          <button type="button" className="btn ghost" onClick={onClose}>
            {strings.settings.close}
          </button>
        </div>
        <div className="panel-bd" style={{ display: "grid", gap: 16 }}>
          {settingsRows.map((row) => (
            <div key={row.label} style={{ display: "grid", gap: 8 }}>
              <div style={{ font: "11px/1 var(--mono)", letterSpacing: ".12em", textTransform: "uppercase", color: "var(--text-dim)" }}>
                {row.label}
              </div>
              {row.content}
            </div>
          ))}
        </div>
      </div>
    </div>
  );
}
