import { useMemo, useRef, useState } from "react";

import {
  pausePlayback,
  pauseRecording,
  playPlayback,
  resumeRecording,
  seekPlayback,
  startRecording,
  stopPlayback,
  stopRecording,
} from "../../api/client";
import { formatSessionStateLabel, getTranslations } from "../../i18n";
import { useSessionStore } from "../../stores/sessionStore";
import type { Locale } from "../../types";
import {
  calculateSeekTimestamp,
  formatTransportTime,
  resolveTransportCommand,
  type TransportCommand,
} from "./transportBarModel";
import {
  getDisplayedSeekPositionNs,
  getPositionPercent,
} from "../seekModel";

interface TransportBarProps {
  locale: Locale;
  onError: (message: string | null) => void;
}

const DEFAULT_RECORDING_CONFIG = {
  output_path: "capture.rpcap",
  protocols: ["UDP", "TCP"],
};

export function TransportBar({ locale, onError }: TransportBarProps) {
  const state = useSessionStore((s) => s.state);
  const positionNs = useSessionStore((s) => s.position_ns);
  const durationNs = useSessionStore((s) => s.duration_ns);
  const speed = useSessionStore((s) => s.speed);
  const progressRef = useRef<HTMLButtonElement | null>(null);
  const dragPointerIdRef = useRef<number | null>(null);
  const [busy, setBusy] = useState(false);
  const [previewPositionNs, setPreviewPositionNs] = useState<number | null>(null);
  const t = getTranslations(locale);

  const displayedPositionNs = useMemo(
    () => getDisplayedSeekPositionNs(positionNs, previewPositionNs, durationNs),
    [durationNs, positionNs, previewPositionNs],
  );
  const progressPercent = useMemo(
    () => getPositionPercent(displayedPositionNs, durationNs),
    [displayedPositionNs, durationNs],
  );

  const executeCommand = async (command: TransportCommand | null) => {
    if (!command || busy) {
      return;
    }

    const operations: Record<TransportCommand, () => Promise<unknown>> = {
      startRecording: () => startRecording(DEFAULT_RECORDING_CONFIG),
      pauseRecording,
      resumeRecording,
      playPlayback: () => playPlayback(speed),
      pausePlayback,
      stopRecording,
      stopPlayback,
    };

    try {
      setBusy(true);
      onError(null);
      await operations[command]();
    } catch (error) {
      onError(error instanceof Error ? error.message : t.errors.transportCommand);
    } finally {
      setBusy(false);
    }
  };

  const getTimestampFromClientX = (clientX: number) => {
    if (!progressRef.current) {
      return 0;
    }

    const rect = progressRef.current.getBoundingClientRect();
    return calculateSeekTimestamp({
      clientX,
      left: rect.left,
      width: rect.width,
      durationNs,
    });
  };

  const commitSeek = async (timestampNs: number) => {
    if (busy || durationNs <= 0) {
      setPreviewPositionNs(null);
      return;
    }

    if (busy || !progressRef.current || durationNs <= 0) {
      return;
    }

    try {
      setBusy(true);
      setPreviewPositionNs(timestampNs);
      onError(null);
      await seekPlayback(timestampNs);
    } catch (error) {
      onError(error instanceof Error ? error.message : t.errors.seekCommand);
    } finally {
      setBusy(false);
      setPreviewPositionNs(null);
    }
  };

  return (
    <div className="flex items-center gap-4 border-b border-t border-hmi-border bg-hmi-surface px-4 py-2">
      <div className="flex items-center gap-2">
        <button
          type="button"
          disabled={busy}
          onClick={() => executeCommand(resolveTransportCommand("record", state))}
          className="flex h-8 w-8 items-center justify-center rounded-full border border-hmi-danger/50 bg-hmi-danger/20 text-[10px] text-hmi-danger hover:bg-hmi-danger/30 disabled:opacity-50"
          title={t.transport.record}
        >
          REC
        </button>
        <button
          type="button"
          disabled={busy}
          onClick={() => executeCommand(resolveTransportCommand("play", state))}
          className="flex h-8 w-8 items-center justify-center rounded border border-hmi-border bg-hmi-surface-alt text-[10px] text-hmi-text hover:bg-hmi-border disabled:opacity-50"
          title={t.transport.play}
        >
          PLAY
        </button>
        <button
          type="button"
          disabled={busy}
          onClick={() => executeCommand(resolveTransportCommand("pause", state))}
          className="flex h-8 w-8 items-center justify-center rounded border border-hmi-border bg-hmi-surface-alt text-[10px] text-hmi-text hover:bg-hmi-border disabled:opacity-50"
          title={t.transport.pause}
        >
          PAUSE
        </button>
        <button
          type="button"
          disabled={busy}
          onClick={() => executeCommand(resolveTransportCommand("stop", state))}
          className="flex h-8 w-8 items-center justify-center rounded border border-hmi-border bg-hmi-surface-alt text-[10px] text-hmi-text hover:bg-hmi-border disabled:opacity-50"
          title={t.transport.stop}
        >
          STOP
        </button>
      </div>

      <div className="flex items-center gap-2 font-mono text-sm">
        <span className="text-hmi-accent">{formatTransportTime(positionNs)}</span>
        <span className="text-hmi-text-muted">/</span>
        <span className="text-hmi-text-muted">{formatTransportTime(durationNs)}</span>
      </div>

      <button
        ref={progressRef}
        type="button"
        disabled={busy || durationNs <= 0}
        className="touch-none flex-1 rounded-full bg-hmi-surface-alt disabled:cursor-not-allowed"
        onPointerDown={(event) => {
          if (busy || durationNs <= 0) {
            return;
          }

          dragPointerIdRef.current = event.pointerId;
          event.currentTarget.setPointerCapture(event.pointerId);
          setPreviewPositionNs(getTimestampFromClientX(event.clientX));
        }}
        onPointerMove={(event) => {
          if (dragPointerIdRef.current !== event.pointerId) {
            return;
          }

          setPreviewPositionNs(getTimestampFromClientX(event.clientX));
        }}
        onPointerUp={(event) => {
          if (dragPointerIdRef.current !== event.pointerId) {
            return;
          }

          dragPointerIdRef.current = null;
          if (event.currentTarget.hasPointerCapture(event.pointerId)) {
            event.currentTarget.releasePointerCapture(event.pointerId);
          }
          void commitSeek(getTimestampFromClientX(event.clientX));
        }}
        onPointerCancel={() => {
          dragPointerIdRef.current = null;
          setPreviewPositionNs(null);
        }}
        title={t.transport.seek}
      >
        <div className="h-2 overflow-hidden rounded-full">
          <div
            className="h-full bg-hmi-accent transition-all"
            style={{ width: `${progressPercent}%` }}
          />
        </div>
      </button>

      <span className="text-xs text-hmi-text-muted">{speed.toFixed(1)}x</span>
      <span className="rounded border border-hmi-border bg-hmi-surface-alt px-2 py-0.5 text-xs text-hmi-accent">
        {formatSessionStateLabel(state, locale)}
      </span>
    </div>
  );
}
