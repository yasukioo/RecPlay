import { getTranslations } from "../../i18n";
import type { Locale, ThemeMode } from "../../types";

export type WsStatus = "connecting" | "open" | "closed" | "error";

export type MenuKey = "file" | "view" | "plugins" | "help";

export interface ConnectionMeta {
  label: string;
  tone: "success" | "warning" | "muted" | "danger";
}

export interface ThemeToggleMeta {
  icon: "sun" | "moon";
  label: string;
  title: string;
}

export interface LocaleToggleMeta {
  label: string;
  title: string;
  nextLocale: Locale;
}

export interface MenuAction {
  label: string;
  shortcut?: string;
  description: string;
}

export interface MenuSection {
  key: MenuKey;
  label: string;
  items: MenuAction[];
}

export function toggleMenu(current: MenuKey | null, next: MenuKey): MenuKey | null {
  return current === next ? null : next;
}

export function getMenuSections(locale: Locale): MenuSection[] {
  const t = getTranslations(locale);

  return [
    {
      key: "file",
      label: t.menu.file,
      items: [
        { ...t.menu.items.openCapture, shortcut: "Ctrl+O" },
        { ...t.menu.items.recentSessions },
        { ...t.menu.items.exportSnapshot, shortcut: "Ctrl+E" },
      ],
    },
    {
      key: "view",
      label: t.menu.view,
      items: [
        { ...t.menu.items.resetLayout },
        { ...t.menu.items.timelineFocus, shortcut: "Alt+1" },
        { ...t.menu.items.diagnosticsOverlay },
      ],
    },
    {
      key: "plugins",
      label: t.menu.plugins,
      items: [
        { ...t.menu.items.refreshCatalog, shortcut: "F5" },
        { ...t.menu.items.scanBundles },
        { ...t.menu.items.routeInspector },
      ],
    },
    {
      key: "help",
      label: t.menu.help,
      items: [
        { ...t.menu.items.keyboardMap },
        { ...t.menu.items.httpApi },
        { ...t.menu.items.aboutRecPlay },
      ],
    },
  ];
}

export function getConnectionMeta(status: WsStatus, locale: Locale): ConnectionMeta {
  const t = getTranslations(locale);

  switch (status) {
    case "open":
      return { label: t.connectionStatus.open, tone: "success" };
    case "connecting":
      return { label: t.connectionStatus.connecting, tone: "warning" };
    case "error":
      return { label: t.connectionStatus.error, tone: "danger" };
    case "closed":
    default:
      return { label: t.connectionStatus.closed, tone: "muted" };
  }
}

export function getThemeToggleMeta(theme: ThemeMode, locale: Locale): ThemeToggleMeta {
  const t = getTranslations(locale);

  if (theme === "light") {
    return {
      icon: "moon",
      label: t.menu.theme.dark,
      title: t.menu.theme.switchToDark,
    };
  }

  return {
    icon: "sun",
    label: t.menu.theme.light,
    title: t.menu.theme.switchToLight,
  };
}

export function getLocaleToggleMeta(locale: Locale): LocaleToggleMeta {
  const t = getTranslations(locale);

  if (locale === "zh-CN") {
    return {
      label: t.menu.locale.englishShort,
      title: t.menu.locale.toggleEnglish,
      nextLocale: "en-US",
    };
  }

  return {
    label: t.menu.locale.chineseShort,
    title: t.menu.locale.toggleChinese,
    nextLocale: "zh-CN",
  };
}
