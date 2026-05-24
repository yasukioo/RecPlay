import { useMemo, useState } from "react";

import { formatPluginStateLabel, getTranslations } from "../../i18n";
import { getPluginCardClasses } from "../../layoutModel";
import { useLocaleStore } from "../../stores/localeStore";
import type { PluginInfo } from "../../types";
import { usePluginStore } from "../../stores/pluginStore";
import {
  buildPluginLogEntry,
  getPluginActionLabel,
  getPluginConfigFields,
} from "./pluginCardModel";

interface PluginCardProps {
  plugin: PluginInfo | null;
  onTogglePlugin?: (pluginId: string) => void | Promise<void>;
  onSaveConfig?: (
    pluginId: string,
    configFields: NonNullable<PluginInfo["config_fields"]>,
  ) => void | Promise<void>;
}

export function PluginCard({ plugin, onTogglePlugin, onSaveConfig }: PluginCardProps) {
  const locale = useLocaleStore((s) => s.locale);
  const togglePluginEnabled = usePluginStore((s) => s.togglePluginEnabled);

  const [draftValues, setDraftValues] = useState<Record<string, string>>({});
  const [logs, setLogs] = useState<string[]>([]);
  const layoutClasses = getPluginCardClasses();
  const t = getTranslations(locale);
  const configFields = useMemo(() => (plugin ? getPluginConfigFields(plugin, locale) : []), [locale, plugin]);
  const actionLabel = plugin ? getPluginActionLabel(plugin.state, locale) : t.pluginCard.actions.error;

  if (!plugin) {
    return (
      <div className="flex h-full items-center justify-center text-sm text-hmi-text-muted">
        {t.pluginCard.empty}
      </div>
    );
  }

  const handlePrimaryAction = () => {
    const nextEntry = buildPluginLogEntry(plugin.id, actionLabel);
    setLogs((current) => [nextEntry, ...current].slice(0, 8));

    if (plugin.state !== "error") {
      if (onTogglePlugin) {
        void onTogglePlugin(plugin.id);
      } else {
        togglePluginEnabled(plugin.id);
      }
    }
  };

  const stateBadgeClass =
    plugin.state === "active"
      ? "bg-hmi-success/20 text-hmi-success"
      : plugin.state === "error"
        ? "bg-hmi-danger/20 text-hmi-danger"
        : "bg-hmi-surface-alt text-hmi-text-muted";

  return (
    <div className="space-y-4 rounded border border-hmi-border bg-hmi-surface p-4">
      <div className="flex items-center justify-between">
        <div>
          <h3 className="font-bold text-hmi-text">{plugin.name}</h3>
          <p className="text-xs text-hmi-text-muted">{t.pluginCard.subtitle}</p>
        </div>
        <span className={`rounded px-2 py-0.5 text-xs ${stateBadgeClass}`}>
          {formatPluginStateLabel(plugin.state, locale)}
        </span>
      </div>

      <dl className="space-y-1 text-sm">
        <div className="flex gap-2">
          <dt className="text-hmi-text-muted">{t.pluginCard.id}:</dt>
          <dd className="font-mono text-xs">{plugin.id}</dd>
        </div>
        <div className="flex gap-2">
          <dt className="text-hmi-text-muted">{t.pluginCard.version}:</dt>
          <dd>{plugin.version}</dd>
        </div>
        <div className="flex gap-2">
          <dt className="text-hmi-text-muted">{t.pluginCard.path}:</dt>
          <dd className="truncate font-mono text-xs">{plugin.bundle_path}</dd>
        </div>
      </dl>

      <div className={layoutClasses.configGrid}>
        {configFields.map((field) => (
          <label key={field.key} className="space-y-1">
            <span className="text-xs text-hmi-text-muted">{field.label}</span>
            <input
              value={draftValues[field.key] ?? field.value}
              onChange={(event) =>
                setDraftValues((current) => ({ ...current, [field.key]: event.target.value }))
              }
              className="w-full rounded border border-hmi-border bg-hmi-bg px-2 py-1.5 text-sm text-hmi-text"
            />
          </label>
        ))}
      </div>

      <div className={layoutClasses.actions}>
        <button
          type="button"
          onClick={handlePrimaryAction}
          className="rounded border border-hmi-border bg-hmi-surface-alt px-3 py-1.5 text-xs text-hmi-text hover:bg-hmi-border"
        >
          {actionLabel}
        </button>
        <button
          type="button"
          onClick={() => {
            const nextEntry = buildPluginLogEntry(plugin.id, t.pluginCard.saveConfig);
            setLogs((current) => [nextEntry, ...current].slice(0, 8));
            const nextConfigFields = configFields.map((field) => ({
              key: field.key,
              label: field.label,
              value: draftValues[field.key] ?? field.value,
            }));
            if (onSaveConfig) {
              void onSaveConfig(plugin.id, nextConfigFields);
            }
          }}
          className="rounded border border-hmi-border bg-hmi-surface-alt px-3 py-1.5 text-xs text-hmi-text hover:bg-hmi-border"
        >
          {t.pluginCard.saveConfig}
        </button>
      </div>

      <div className="space-y-2 border-t border-hmi-border pt-3">
        <div className="flex items-center justify-between">
          <h4 className="text-xs font-bold uppercase tracking-wide text-hmi-text-muted">{t.pluginCard.pluginLog}</h4>
          <span className="text-[11px] text-hmi-text-muted">{logs.length} {t.pluginCard.entries}</span>
        </div>
        <div className="max-h-40 space-y-1 overflow-y-auto rounded border border-hmi-border bg-hmi-bg p-2">
          {logs.length === 0 && (
            <div className="text-xs text-hmi-text-muted">{t.pluginCard.noLogEvents}</div>
          )}
          {logs.map((entry) => (
            <div key={entry} className="font-mono text-xs text-hmi-text-muted">
              {entry}
            </div>
          ))}
        </div>
      </div>
    </div>
  );
}
