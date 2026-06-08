import { useState } from "react";
import {
  pauseRecording,
  resumeRecording,
  startRecording,
  stopRecording,
} from "../../api/client";
import { formatSessionStateLabel, getTranslations } from "../../i18n";
import { useLocaleStore } from "../../stores/localeStore";
import { useSessionStore } from "../../stores/sessionStore";
import type { SessionState } from "../../types";

interface RecordPanelProps {
  onError: (message: string | null) => void;
}

export function RecordPanel({ onError }: RecordPanelProps) {
  const locale = useLocaleStore((s) => s.locale);
  const state = useSessionStore((s) => s.state);
  const [outputPath, setOutputPath] = useState("capture.rpcap");
  const [protocols, setProtocols] = useState("UDP,TCP");
  const [busy, setBusy] = useState(false);
  const t = getTranslations(locale);
  const statusClasses = getRecordStatusClasses(state);
  const panelClasses = getRecordPanelClasses(state);

  const run = async (operation: () => Promise<unknown>) => {
    try {
      setBusy(true);
      onError(null);
      await operation();
    } catch (error) {
      onError(error instanceof Error ? error.message : t.errors.recordCommand);
    } finally {
      setBusy(false);
    }
  };

  return (
    <div className={panelClasses}>
      <h3 className="text-sm font-bold text-hmi-accent uppercase tracking-wide">{t.recordPanel.title}</h3>

      <div
        role="status"
        aria-live="polite"
        className={`flex items-center justify-between gap-3 rounded border px-3 py-2 text-sm ${statusClasses}`}
      >
        <div className="flex min-w-0 items-center gap-2">
          <span aria-hidden="true" className="h-2.5 w-2.5 rounded-full bg-current" />
          <span className="text-xs uppercase tracking-wide opacity-80">{t.sessionInfo.state}</span>
        </div>
        <span className="truncate font-semibold">{formatSessionStateLabel(state, locale)}</span>
      </div>

      <div className="grid grid-cols-1 gap-3 md:grid-cols-2">
        <label className="space-y-1">
          <span className="text-xs text-hmi-text-muted">{t.recordPanel.outputPath}</span>
          <input
            value={outputPath}
            onChange={(e) => setOutputPath(e.target.value)}
            className="w-full px-2 py-1.5 rounded bg-hmi-bg border border-hmi-border text-sm text-hmi-text"
          />
        </label>
        <label className="space-y-1">
          <span className="text-xs text-hmi-text-muted">{t.recordPanel.protocols}</span>
          <input
            value={protocols}
            onChange={(e) => setProtocols(e.target.value)}
            className="w-full px-2 py-1.5 rounded bg-hmi-bg border border-hmi-border text-sm text-hmi-text"
          />
        </label>
      </div>

      <div className="flex flex-wrap gap-2">
        <button
          type="button"
          disabled={busy}
          onClick={() =>
            run(() =>
              startRecording({
                output_path: outputPath,
                protocols: protocols.split(",").map((s) => s.trim()).filter(Boolean),
              }),
            )
          }
          className="px-3 py-1.5 rounded text-xs bg-hmi-success/10 text-hmi-success border border-hmi-success/30 hover:bg-hmi-success/20 disabled:opacity-50"
        >
          {t.recordPanel.start}
        </button>
        <button
          type="button"
          disabled={busy}
          onClick={() => run(pauseRecording)}
          className="px-3 py-1.5 rounded text-xs bg-hmi-surface-alt border border-hmi-border text-hmi-text hover:bg-hmi-border disabled:opacity-50"
        >
          {t.recordPanel.pause}
        </button>
        <button
          type="button"
          disabled={busy}
          onClick={() => run(resumeRecording)}
          className="px-3 py-1.5 rounded text-xs bg-hmi-surface-alt border border-hmi-border text-hmi-text hover:bg-hmi-border disabled:opacity-50"
        >
          {t.recordPanel.resume}
        </button>
        <button
          type="button"
          disabled={busy}
          onClick={() => run(stopRecording)}
          className="px-3 py-1.5 rounded text-xs bg-hmi-danger/10 text-hmi-danger border border-hmi-danger/30 hover:bg-hmi-danger/20 disabled:opacity-50"
        >
          {t.recordPanel.stop}
        </button>
      </div>
    </div>
  );
}

function getRecordPanelClasses(state: SessionState): string {
  switch (state) {
    case "Recording":
      return "space-y-3 rounded border border-hmi-danger/40 bg-hmi-surface p-4 shadow-[0_0_0_1px_rgba(239,83,80,0.08)]";
    case "RecordingPaused":
      return "space-y-3 rounded border border-hmi-warning/40 bg-hmi-surface p-4 shadow-[0_0_0_1px_rgba(255,167,38,0.08)]";
    default:
      return "space-y-3 rounded border border-hmi-border bg-hmi-surface p-4";
  }
}

function getRecordStatusClasses(state: SessionState): string {
  switch (state) {
    case "Recording":
      return "border-hmi-danger/40 bg-hmi-danger/10 text-hmi-danger";
    case "RecordingPaused":
      return "border-hmi-warning/40 bg-hmi-warning/10 text-hmi-warning";
    case "Playing":
    case "PlaybackPaused":
    case "Seeking":
      return "border-hmi-accent/40 bg-hmi-accent/10 text-hmi-accent";
    case "Idle":
      return "border-hmi-success/30 bg-hmi-success/10 text-hmi-success";
    case "Unknown":
    default:
      return "border-hmi-border bg-hmi-surface-alt text-hmi-text-muted";
  }
}
