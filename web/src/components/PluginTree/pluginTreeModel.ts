import { getTranslations } from "../../i18n";
import type { Locale, PluginInfo } from "../../types";

export interface PluginMenuAction {
  key: "select" | "toggle";
  label: string;
}

export function getPluginMenuActions(
  state: PluginInfo["state"],
  locale: Locale = "en-US",
): PluginMenuAction[] {
  const t = getTranslations(locale);
  const actions: PluginMenuAction[] = [{ key: "select", label: t.pluginTree.select }];

  if (state === "active") {
    actions.push({ key: "toggle", label: t.pluginTree.disable });
  } else if (state === "inactive") {
    actions.push({ key: "toggle", label: t.pluginTree.enable });
  }

  return actions;
}
