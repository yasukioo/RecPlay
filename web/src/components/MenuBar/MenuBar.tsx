import { useEffect, useMemo, useRef, useState } from "react";

import { APP_VERSION, getTranslations } from "../../i18n";
import type { Locale, ThemeMode } from "../../types";
import {
  getConnectionMeta,
  getLocaleToggleMeta,
  getMenuSections,
  getThemeToggleMeta,
  toggleMenu,
  type MenuKey,
  type WsStatus,
} from "./menuBarModel";

interface MenuBarProps {
  wsStatus: WsStatus;
  theme: ThemeMode;
  locale: Locale;
  onToggleTheme: () => void;
  onToggleLocale: () => void;
}

export function MenuBar({ wsStatus, theme, locale, onToggleTheme, onToggleLocale }: MenuBarProps) {
  const [activeMenu, setActiveMenu] = useState<MenuKey | null>(null);
  const rootRef = useRef<HTMLElement | null>(null);
  const translations = useMemo(() => getTranslations(locale), [locale]);
  const menuSections = useMemo(() => getMenuSections(locale), [locale]);
  const connectionMeta = useMemo(() => getConnectionMeta(wsStatus, locale), [locale, wsStatus]);
  const themeToggleMeta = useMemo(() => getThemeToggleMeta(theme, locale), [locale, theme]);
  const localeToggleMeta = useMemo(() => getLocaleToggleMeta(locale), [locale]);

  useEffect(() => {
    if (!activeMenu) {
      return undefined;
    }

    const handlePointerDown = (event: MouseEvent) => {
      if (!rootRef.current?.contains(event.target as Node)) {
        setActiveMenu(null);
      }
    };

    const handleKeyDown = (event: KeyboardEvent) => {
      if (event.key === "Escape") {
        setActiveMenu(null);
      }
    };

    window.addEventListener("mousedown", handlePointerDown);
    window.addEventListener("keydown", handleKeyDown);

    return () => {
      window.removeEventListener("mousedown", handlePointerDown);
      window.removeEventListener("keydown", handleKeyDown);
    };
  }, [activeMenu]);

  const toneClasses: Record<string, string> = {
    success: "border-hmi-success/50 bg-hmi-success/10 text-hmi-success",
    warning: "border-hmi-warning/50 bg-hmi-warning/10 text-hmi-warning",
    muted: "border-hmi-border bg-hmi-surface-alt text-hmi-text-muted",
    danger: "border-hmi-danger/50 bg-hmi-danger/10 text-hmi-danger",
  };

  return (
    <header
      ref={rootRef}
      className="flex flex-wrap items-center justify-between gap-x-4 gap-y-2 border-b border-hmi-border bg-hmi-surface px-3 py-2 sm:px-4"
    >
      <div className="flex min-w-0 flex-1 flex-wrap items-center gap-3 sm:gap-4">
        <span className="text-lg font-bold text-hmi-accent">RecPlay</span>
        <nav className="flex flex-wrap gap-1 text-sm text-hmi-text-muted">
          {menuSections.map((section) => {
            const open = activeMenu === section.key;

            return (
              <div key={section.key} className="relative">
                <button
                  type="button"
                  aria-haspopup="menu"
                  aria-expanded={open}
                  onClick={() => setActiveMenu((current) => toggleMenu(current, section.key))}
                  className={[
                    "rounded border px-2 py-1 transition-colors",
                    open
                      ? "border-hmi-accent/60 bg-hmi-surface-alt text-hmi-text"
                      : "border-transparent hover:border-hmi-border hover:text-hmi-text",
                  ].join(" ")}
                >
                  {section.label}
                </button>
                {open && (
                  <div className="absolute left-0 top-full z-20 mt-2 w-[min(18rem,calc(100vw-2rem))] rounded-md border border-hmi-border bg-hmi-surface-alt p-2 shadow-lg shadow-black/30 sm:w-72">
                    <div className="px-2 pb-2 text-[11px] uppercase tracking-wide text-hmi-text-muted">
                      {section.label}
                    </div>
                    <div className="space-y-1">
                      {section.items.map((item) => (
                        <button
                          key={item.label}
                          type="button"
                          className="flex w-full items-start justify-between gap-3 rounded px-2 py-2 text-left hover:bg-hmi-border/40"
                          onClick={() => setActiveMenu(null)}
                        >
                          <span className="space-y-1">
                            <span className="block text-sm text-hmi-text">{item.label}</span>
                            <span className="block text-xs text-hmi-text-muted">{item.description}</span>
                          </span>
                          {item.shortcut && (
                            <span className="pt-0.5 text-[11px] text-hmi-text-muted">{item.shortcut}</span>
                          )}
                        </button>
                      ))}
                    </div>
                  </div>
                )}
              </div>
            );
          })}
        </nav>
      </div>
      <div className="flex w-full flex-wrap items-center justify-end gap-2 text-xs text-hmi-text-muted sm:w-auto">
        <span className={`inline-flex items-center gap-2 rounded-full border px-2 py-1 ${toneClasses[connectionMeta.tone]}`}>
          <span className="h-2 w-2 rounded-full bg-current" />
          <span>{connectionMeta.label}</span>
        </span>
        <button
          type="button"
          onClick={onToggleLocale}
          className="inline-flex h-8 min-w-10 items-center justify-center rounded border border-hmi-border bg-hmi-surface-alt px-2 text-hmi-text-muted transition-colors hover:text-hmi-text"
          aria-label={localeToggleMeta.title}
          title={localeToggleMeta.title}
        >
          <span className="text-[11px] font-semibold">{localeToggleMeta.label}</span>
        </button>
        <button
          type="button"
          onClick={onToggleTheme}
          className="inline-flex h-8 w-8 items-center justify-center rounded border border-hmi-border bg-hmi-surface-alt text-hmi-text-muted transition-colors hover:text-hmi-text"
          aria-label={themeToggleMeta.label}
          aria-pressed={theme === "light"}
          title={themeToggleMeta.title}
        >
          {themeToggleMeta.icon === "sun" ? (
            <svg viewBox="0 0 24 24" className="h-4 w-4 fill-none stroke-current" strokeWidth="1.7">
              <circle cx="12" cy="12" r="3.5" />
              <path d="M12 2.75v2.5M12 18.75v2.5M21.25 12h-2.5M5.25 12h-2.5M18.54 5.46l-1.77 1.77M7.23 16.77l-1.77 1.77M18.54 18.54l-1.77-1.77M7.23 7.23 5.46 5.46" />
            </svg>
          ) : (
            <svg viewBox="0 0 24 24" className="h-4 w-4 fill-none stroke-current" strokeWidth="1.7">
              <path d="M20 15.2A7.7 7.7 0 1 1 12.8 4 6.2 6.2 0 0 0 20 15.2Z" />
            </svg>
          )}
        </button>
        <button
          type="button"
          className="inline-flex h-8 w-8 items-center justify-center rounded border border-hmi-border bg-hmi-surface-alt text-hmi-text-muted transition-colors hover:text-hmi-text"
          aria-label={translations.menu.settings}
          title={translations.menu.settings}
        >
          <svg viewBox="0 0 24 24" className="h-4 w-4 fill-none stroke-current" strokeWidth="1.7">
            <path d="M12 3.75 13.4 6l2.56.45-.93 2.42 1.7 1.96-1.7 1.96.93 2.42-2.56.45L12 20.25l-1.4-2.25-2.56-.45.93-2.42-1.7-1.96 1.7-1.96-.93-2.42L10.6 6 12 3.75Z" />
            <circle cx="12" cy="12" r="2.4" />
          </svg>
        </button>
        <span>{APP_VERSION}</span>
      </div>
    </header>
  );
}
