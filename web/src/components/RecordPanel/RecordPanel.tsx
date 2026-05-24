import { useState } from "react";
import {
  pauseRecording,
  resumeRecording,
  startRecording,
  stopRecording,
} from "../../api/client";
import { getTranslations } from "../../i18n";
import { useLocaleStore } from "../../stores/localeStore";

interface RecordPanelProps {
  onError: (message: string | null) => void;
}

export function RecordPanel({ onError }: RecordPanelProps) {
  const locale = useLocaleStore((s) => s.locale);
  const [outputPath, setOutputPath] = useState("capture.rpcap");
  const [protocols, setProtocols] = useState("UDP,TCP");
  const [busy, setBusy] = useState(false);
  const t = getTranslations(locale);

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
    <div className="p-4 bg-hmi-surface rounded border border-hmi-border space-y-3">
      <h3 className="text-sm font-bold text-hmi-accent uppercase tracking-wide">{t.recordPanel.title}</h3>

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
