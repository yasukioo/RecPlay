import { getTranslations } from "../../i18n";
import type { Locale, PluginInfo } from "../../types";

export interface PluginConfigField {
  key: string;
  label: string;
  value: string;
}

export function getPluginActionLabel(
  state: PluginInfo["state"],
  locale: Locale = "en-US",
): string {
  const t = getTranslations(locale);

  switch (state) {
    case "active":
      return t.pluginCard.actions.active;
    case "inactive":
      return t.pluginCard.actions.inactive;
    case "error":
    default:
      return t.pluginCard.actions.error;
  }
}

export function getPluginConfigFields(
  plugin: PluginInfo,
  locale: Locale = "en-US",
): PluginConfigField[] {
  if (plugin.config_fields && plugin.config_fields.length > 0) {
    return plugin.config_fields.map((field) => ({
      key: field.key,
      label: field.label,
      value: field.value,
    }));
  }

  const key = `${plugin.id} ${plugin.name}`.toLowerCase();
  const t = getTranslations(locale);

  if (key.includes("tcp")) {
    return [
      { key: "endpoint", label: t.pluginCard.configLabels.endpoint, value: "127.0.0.1:9000" },
      { key: "protocol", label: t.pluginCard.configLabels.protocol, value: "TCP" },
      { key: "buffer", label: t.pluginCard.configLabels.buffer, value: t.pluginCard.configValues.frames4096 },
    ];
  }

  if (key.includes("udp")) {
    return [
      { key: "endpoint", label: t.pluginCard.configLabels.endpoint, value: "239.10.10.1:5000" },
      { key: "protocol", label: t.pluginCard.configLabels.protocol, value: "UDP" },
      { key: "ttl", label: t.pluginCard.configLabels.ttl, value: "8" },
    ];
  }

  return [
    {
      key: "mode",
      label: t.pluginCard.configLabels.mode,
      value: plugin.state === "active" ? t.pluginCard.configValues.online : t.pluginCard.configValues.standby,
    },
    { key: "profile", label: t.pluginCard.configLabels.profile, value: t.pluginCard.configValues.defaultProfile },
    {
      key: "health",
      label: t.pluginCard.configLabels.health,
      value: plugin.state === "error" ? t.pluginCard.configValues.faulted : t.pluginCard.configValues.nominal,
    },
  ];
}

export function buildPluginLogEntry(pluginId: string, actionLabel: string): string {
  return `${new Date().toISOString()} [${pluginId}] ${actionLabel}`;
}
