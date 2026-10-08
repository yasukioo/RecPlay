import { useState } from "react";
import {
  openPlaybackFile,
  pausePlayback,
  playPlayback,
  seekPlayback,
  setPlaybackLoop,
  setPlaybackSpeed,
  stopPlayback,
} from "../../api/client";
import { getTranslations } from "../../i18n";
import { useLocaleStore } from "../../stores/localeStore";
import { useSessionStore } from "../../stores/sessionStore";

interface PlaybackPanelProps {
  onError: (message: string | null) => void;
}

export function PlaybackPanel({ onError }: PlaybackPanelProps) {
  const locale = useLocaleStore((s) => s.locale);
  const setLoopRegion = useSessionStore((s) => s.setLoopRegion);
  const [filePath, setFilePath] = useState("capture.rpcap");
  const [speed, setSpeed] = useState("1");
  const [seekNs, setSeekNs] = useState("0");
  const [loopStartNs, setLoopStartNs] = useState("0");
  const [loopEndNs, setLoopEndNs] = useState("1000000000");
  const [busy, setBusy] = useState(false);
  const t = getTranslations(locale);

  const run = async (operation: () => Promise<unknown>) => {
    try {
      setBusy(true);
      onError(null);
      await operation();
    } catch (error) {
      onError(error instanceof Error ? error.message : t.errors.playbackCommand);
    } finally {
      setBusy(false);
    }
  };

  const inputCls = "w-full px-2 py-1.5 rounded bg-hmi-bg border border-hmi-border text-sm text-hmi-text";
  const btnCls = "px-3 py-1.5 rounded text-xs bg-hmi-surface-alt border border-hmi-border text-hmi-text hover:bg-hmi-border disabled:opacity-50";
  const loopStartValue = Math.max(0, Number(loopStartNs) || 0);
  const loopEndValue = Math.max(0, Number(loopEndNs) || 0);

  return (
    <div className="p-4 bg-hmi-surface rounded border border-hmi-border space-y-3">
      <h3 className="text-sm font-bold text-hmi-accent uppercase tracking-wide">{t.playbackPanel.title}</h3>

      <div className="grid grid-cols-1 gap-3 md:grid-cols-2">
        <label className="space-y-1 md:col-span-2">
          <span className="text-xs text-hmi-text-muted">{t.playbackPanel.filePath}</span>
          <input value={filePath} onChange={(e) => setFilePath(e.target.value)} className={inputCls} />
        </label>
        <label className="space-y-1">
          <span className="text-xs text-hmi-text-muted">{t.playbackPanel.speed}</span>
          <input value={speed} onChange={(e) => setSpeed(e.target.value)} className={inputCls} />
        </label>
        <label className="space-y-1">
          <span className="text-xs text-hmi-text-muted">{t.playbackPanel.seekNs}</span>
          <input value={seekNs} onChange={(e) => setSeekNs(e.target.value)} className={inputCls} />
        </label>
        <label className="space-y-1">
          <span className="text-xs text-hmi-text-muted">{t.playbackPanel.loopStart}</span>
          <input value={loopStartNs} onChange={(e) => setLoopStartNs(e.target.value)} className={inputCls} />
        </label>
        <label className="space-y-1">
          <span className="text-xs text-hmi-text-muted">{t.playbackPanel.loopEnd}</span>
          <input value={loopEndNs} onChange={(e) => setLoopEndNs(e.target.value)} className={inputCls} />
        </label>
      </div>

      <div className="flex flex-wrap gap-2">
        <button type="button" disabled={busy} onClick={() => run(() => openPlaybackFile(filePath))} className={btnCls}>{t.playbackPanel.open}</button>
        <button type="button" disabled={busy} onClick={() => run(() => playPlayback(Number(speed) || 1))} className={btnCls}>{t.playbackPanel.play}</button>
        <button type="button" disabled={busy} onClick={() => run(pausePlayback)} className={btnCls}>{t.playbackPanel.pause}</button>
        <button type="button" disabled={busy} onClick={() => run(() => setPlaybackSpeed(Number(speed) || 1))} className={btnCls}>{t.playbackPanel.speedAction}</button>
        <button type="button" disabled={busy} onClick={() => run(() => seekPlayback(Math.max(0, Number(seekNs) || 0)))} className={btnCls}>{t.playbackPanel.seekAction}</button>
        <button
          type="button"
          disabled={busy}
          onClick={() =>
            run(async () => {
              await setPlaybackLoop(loopStartValue, loopEndValue);
              setLoopRegion(loopStartValue, loopEndValue);
            })
          }
          className={btnCls}
        >
          {t.playbackPanel.loopAction}
        </button>
        <button type="button" disabled={busy} onClick={() => run(stopPlayback)} className={btnCls}>{t.playbackPanel.stop}</button>
      </div>
    </div>
  );
}
