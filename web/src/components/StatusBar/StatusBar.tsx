import { useEffect, useMemo, useState } from "react";

import { getTranslations } from "../../i18n";
import { getStatusBarClasses } from "../../layoutModel";
import { useStatsStore } from "../../stores/statsStore";
import type { Locale } from "../../types";
import {
  formatCpuUsage,
  formatClock,
  formatDiskQueue,
  formatStorageFree,
  getBufferUsagePercent,
  getStorageFreeBytes,
  getWsTone,
} from "./statusBarModel";

interface StatusBarProps {
  locale: Locale;
  wsStatus: "connecting" | "open" | "closed" | "error";
}

export function StatusBar({ locale, wsStatus }: StatusBarProps) {
  const throughput = useStatsStore((s) => s.total_throughput_mbps);
  const drops = useStatsStore((s) => s.total_drops);
  const ringbufUsed = useStatsStore((s) => s.ringbuf_used);
  const ringbufCapacity = useStatsStore((s) => s.ringbuf_capacity);
  const diskQueueBytes = useStatsStore((s) => s.disk_queue_bytes);
  const cpuUsagePercent = useStatsStore((s) => s.cpu_usage_percent);
  const [clock, setClock] = useState(() => formatClock(new Date()));
  const [storageFreeBytes, setStorageFreeBytes] = useState<number | null>(null);

  useEffect(() => {
    const timer = window.setInterval(() => {
      setClock(formatClock(new Date()));
    }, 1000);

    return () => window.clearInterval(timer);
  }, []);

  useEffect(() => {
    if (typeof navigator === "undefined" || typeof navigator.storage?.estimate !== "function") {
      return undefined;
    }

    let cancelled = false;
    const updateStorage = async () => {
      try {
        const estimate = await navigator.storage?.estimate();
        if (!cancelled) {
          setStorageFreeBytes(getStorageFreeBytes(estimate ?? {}));
        }
      } catch {
        if (!cancelled) {
          setStorageFreeBytes(null);
        }
      }
    };

    void updateStorage();
    const timer = window.setInterval(() => {
      void updateStorage();
    }, 30_000);

    return () => {
      cancelled = true;
      window.clearInterval(timer);
    };
  }, []);

  const wsTone = getWsTone(wsStatus);
  const bufferPercent = useMemo(
    () => getBufferUsagePercent(ringbufUsed, ringbufCapacity),
    [ringbufCapacity, ringbufUsed],
  );
  const layoutClasses = getStatusBarClasses();
  const t = getTranslations(locale);

  const wsToneClass: Record<string, string> = {
    success: "text-hmi-success",
    warning: "text-hmi-warning",
    muted: "text-hmi-text-muted",
    danger: "text-hmi-danger",
  };

  return (
    <footer className={layoutClasses.footer}>
      <div className={layoutClasses.left}>
        <span className={wsToneClass[wsTone]}>
          {t.statusBar.ws}: {t.connectionStatus[wsStatus]}
        </span>
        <span className="text-hmi-text-muted">
          {t.statusBar.throughput}: {throughput.toFixed(2)} Mbps
        </span>
        <span className="text-hmi-text-muted">
          {t.statusBar.ring}: {bufferPercent}%
        </span>
        <span className="text-hmi-text-muted">
          {t.statusBar.diskQueue}: {formatDiskQueue(diskQueueBytes)}
        </span>
        <span className="text-hmi-text-muted">
          {t.statusBar.cpu}: {formatCpuUsage(cpuUsagePercent)}
        </span>
        {drops > 0 && (
          <span className="text-hmi-danger">
            {t.statusBar.drops}: {drops.toLocaleString()}
          </span>
        )}
      </div>
      <div className={layoutClasses.right}>
        <span>{t.statusBar.storageFree}: {formatStorageFree(storageFreeBytes)}</span>
        <span>{clock}</span>
        <span>{t.statusBar.console}</span>
      </div>
    </footer>
  );
}
