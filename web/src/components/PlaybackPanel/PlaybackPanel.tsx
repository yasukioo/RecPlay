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

interface PlaybackPanelProps {
  onError: (message: string | null) => void;
}

export function PlaybackPanel({ onError }: PlaybackPanelProps) {
  const [filePath, setFilePath] = useState("capture.rpcap");
  const [speed, setSpeed] = useState("1");
  const [seekNs, setSeekNs] = useState("0");
  const [loopStartNs, setLoopStartNs] = useState("0");
  const [loopEndNs, setLoopEndNs] = useState("1000000000");
  const [busy, setBusy] = useState(false);

  const run = async (operation: () => Promise<unknown>) => {
    try {
      setBusy(true);
      onError(null);
      await operation();
    } catch (error) {
      onError(error instanceof Error ? error.message : "Playback command failed");
    } finally {
      setBusy(false);
    }
  };

  const inputCls = "w-full px-2 py-1.5 rounded bg-hmi-bg border border-hmi-border text-sm text-hmi-text";
  const btnCls = "px-3 py-1.5 rounded text-xs bg-hmi-surface-alt border border-hmi-border text-hmi-text hover:bg-hmi-border disabled:opacity-50";

  return (
    <div className="p-4 bg-hmi-surface rounded border border-hmi-border space-y-3">
      <h3 className="text-sm font-bold text-hmi-accent uppercase tracking-wide">Playback</h3>

      <div className="grid grid-cols-2 gap-3">
        <label className="space-y-1 col-span-2">
          <span className="text-xs text-hmi-text-muted">File Path</span>
          <input value={filePath} onChange={(e) => setFilePath(e.target.value)} className={inputCls} />
        </label>
        <label className="space-y-1">
          <span className="text-xs text-hmi-text-muted">Speed</span>
          <input value={speed} onChange={(e) => setSpeed(e.target.value)} className={inputCls} />
        </label>
        <label className="space-y-1">
          <span className="text-xs text-hmi-text-muted">Seek (ns)</span>
          <input value={seekNs} onChange={(e) => setSeekNs(e.target.value)} className={inputCls} />
        </label>
        <label className="space-y-1">
          <span className="text-xs text-hmi-text-muted">Loop Start</span>
          <input value={loopStartNs} onChange={(e) => setLoopStartNs(e.target.value)} className={inputCls} />
        </label>
        <label className="space-y-1">
          <span className="text-xs text-hmi-text-muted">Loop End</span>
          <input value={loopEndNs} onChange={(e) => setLoopEndNs(e.target.value)} className={inputCls} />
        </label>
      </div>

      <div className="flex flex-wrap gap-2">
        <button type="button" disabled={busy} onClick={() => run(() => openPlaybackFile(filePath))} className={btnCls}>Open</button>
        <button type="button" disabled={busy} onClick={() => run(() => playPlayback(Number(speed) || 1))} className={btnCls}>Play</button>
        <button type="button" disabled={busy} onClick={() => run(pausePlayback)} className={btnCls}>Pause</button>
        <button type="button" disabled={busy} onClick={() => run(() => setPlaybackSpeed(Number(speed) || 1))} className={btnCls}>Speed</button>
        <button type="button" disabled={busy} onClick={() => run(() => seekPlayback(Math.max(0, Number(seekNs) || 0)))} className={btnCls}>Seek</button>
        <button type="button" disabled={busy} onClick={() => run(() => setPlaybackLoop(Math.max(0, Number(loopStartNs) || 0), Math.max(0, Number(loopEndNs) || 0)))} className={btnCls}>Loop</button>
        <button type="button" disabled={busy} onClick={() => run(stopPlayback)} className={btnCls}>Stop</button>
      </div>
    </div>
  );
}
