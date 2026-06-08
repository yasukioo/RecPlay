import { useEffect, useState } from "react";

import { listRecordingFiles } from "../../api/client";
import { formatSessionStateLabel, getTranslations } from "../../i18n";
import { getSessionInfoClasses } from "../../layoutModel";
import { useLocaleStore } from "../../stores/localeStore";
import { useSessionStore } from "../../stores/sessionStore";

export function SessionInfo() {
  const locale = useLocaleStore((s) => s.locale);
  const state = useSessionStore((s) => s.state);
  const positionNs = useSessionStore((s) => s.position_ns);
  const durationNs = useSessionStore((s) => s.duration_ns);
  const speed = useSessionStore((s) => s.speed);
  const loopStartNs = useSessionStore((s) => s.loop_start_ns);
  const loopEndNs = useSessionStore((s) => s.loop_end_ns);
  const config = useSessionStore((s) => s.config);
  const lastTransition = useSessionStore((s) => s.lastTransition);

  const [files, setFiles] = useState<string[]>([]);
  const layoutClassName = getSessionInfoClasses();
  const t = getTranslations(locale);

  useEffect(() => {
    listRecordingFiles()
      .then((result) => setFiles(result.map((file) => file.name)))
      .catch(() => setFiles([]));
  }, [state]);

  const formatNs = (ns: number) => `${(ns / 1_000_000_000).toFixed(2)}s`;

  const isRecording = state === "Recording" || state === "RecordingPaused";
  const isPlaying =
    state === "Playing" ||
    state === "PlaybackPaused" ||
    state === "Seeking" ||
    state === "Stopped";

  return (
    <aside className={layoutClassName}>
      <h3 className="text-sm font-bold text-hmi-accent uppercase tracking-wide">{t.sessionInfo.title}</h3>

      <dl className="space-y-2 text-sm">
        <div className="flex justify-between">
          <dt className="text-hmi-text-muted">{t.sessionInfo.state}</dt>
          <dd className="font-mono">{formatSessionStateLabel(state, locale)}</dd>
        </div>
        <div className="flex justify-between">
          <dt className="text-hmi-text-muted">{t.sessionInfo.position}</dt>
          <dd className="font-mono">{formatNs(positionNs)}</dd>
        </div>
        <div className="flex justify-between">
          <dt className="text-hmi-text-muted">{t.sessionInfo.duration}</dt>
          <dd className="font-mono">{formatNs(durationNs)}</dd>
        </div>
        <div className="flex justify-between">
          <dt className="text-hmi-text-muted">{t.sessionInfo.speed}</dt>
          <dd className="font-mono">{speed.toFixed(2)}x</dd>
        </div>
      </dl>

      {lastTransition && (
        <div className="rounded border border-hmi-border bg-hmi-surface-alt p-2 text-xs">
          <span className="text-hmi-text-muted">{t.sessionInfo.last}: </span>
          <span>
            {formatSessionStateLabel(lastTransition.old_state as typeof state, locale)}
            {" -> "}
            {formatSessionStateLabel(lastTransition.new_state as typeof state, locale)}
          </span>
        </div>
      )}

      {isRecording && config.recordPath && (
        <div className="space-y-1">
          <h4 className="text-xs font-bold uppercase tracking-wide text-hmi-text-muted">{t.sessionInfo.recordingConfig}</h4>
          <dl className="space-y-1 text-sm">
            <div className="flex justify-between">
              <dt className="text-hmi-text-muted">{t.sessionInfo.output}</dt>
              <dd className="max-w-[140px] truncate font-mono text-xs" title={config.recordPath}>
                {config.recordPath}
              </dd>
            </div>
          </dl>
        </div>
      )}

      {isPlaying && config.playbackPath && (
        <div className="space-y-1">
          <h4 className="text-xs font-bold uppercase tracking-wide text-hmi-text-muted">{t.sessionInfo.playbackConfig}</h4>
          <dl className="space-y-1 text-sm">
            <div className="flex justify-between">
              <dt className="text-hmi-text-muted">{t.sessionInfo.file}</dt>
              <dd className="max-w-[140px] truncate font-mono text-xs" title={config.playbackPath}>
                {config.playbackPath}
              </dd>
            </div>
          </dl>
        </div>
      )}

      {loopStartNs != null && loopEndNs != null && (
        <div className="space-y-1">
          <h4 className="text-xs font-bold uppercase tracking-wide text-hmi-text-muted">{t.sessionInfo.loopRegion}</h4>
          <div className="flex gap-2 text-sm">
            <span className="text-hmi-text-muted">{t.sessionInfo.start}:</span>
            <span className="font-mono">{formatNs(loopStartNs)}</span>
            <span className="text-hmi-text-muted">{t.sessionInfo.end}:</span>
            <span className="font-mono">{formatNs(loopEndNs)}</span>
          </div>
        </div>
      )}

      {files.length > 0 && (
        <div className="space-y-1">
          <h4 className="text-xs font-bold uppercase tracking-wide text-hmi-text-muted">{t.sessionInfo.files}</h4>
          <ul className="space-y-0.5">
            {files.map((file) => (
              <li key={file} className="truncate font-mono text-xs text-hmi-text-muted" title={file}>
                {file}
              </li>
            ))}
          </ul>
        </div>
      )}
    </aside>
  );
}
