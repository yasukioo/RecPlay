import { useState } from "react";

import { formatPluginStateLabel, getTranslations } from "../../i18n";
import { useLocaleStore } from "../../stores/localeStore";
import { usePluginStore } from "../../stores/pluginStore";
import { getPluginMenuActions } from "./pluginTreeModel";

interface PluginTreeProps {
  onTogglePlugin?: (pluginId: string) => void | Promise<void>;
}

export function PluginTree({ onTogglePlugin }: PluginTreeProps) {
  const locale = useLocaleStore((s) => s.locale);
  const plugins = usePluginStore((s) => s.plugins);
  const selectedId = usePluginStore((s) => s.selectedPluginId);
  const selectPlugin = usePluginStore((s) => s.selectPlugin);
  const togglePluginEnabled = usePluginStore((s) => s.togglePluginEnabled);
  const movePlugin = usePluginStore((s) => s.movePlugin);
  const [contextMenu, setContextMenu] = useState<{
    pluginId: string;
    x: number;
    y: number;
  } | null>(null);
  const [draggedId, setDraggedId] = useState<string | null>(null);
  const t = getTranslations(locale);

  const stateColor: Record<string, string> = {
    active: "bg-hmi-success",
    inactive: "bg-hmi-text-muted",
    error: "bg-hmi-danger",
  };

  return (
    <nav className="flex flex-col gap-1 overflow-y-auto rounded border border-hmi-border bg-hmi-surface p-2">
      <h3 className="px-2 py-1 text-xs font-bold uppercase tracking-wide text-hmi-text-muted">
        {t.pluginTree.title}
      </h3>

      {plugins.length === 0 && (
        <span className="px-2 py-4 text-center text-xs text-hmi-text-muted">
          {t.pluginTree.empty}
        </span>
      )}

      {plugins.map((plugin, index) => (
        <div
          key={plugin.id}
          draggable
          onDragStart={() => setDraggedId(plugin.id)}
          onDragOver={(event) => event.preventDefault()}
          onDrop={() => {
            if (draggedId && draggedId !== plugin.id) {
              movePlugin(draggedId, plugin.id);
            }
            setDraggedId(null);
          }}
          className={`rounded border text-sm transition-colors ${
            selectedId === plugin.id
              ? "border-hmi-accent/30 bg-hmi-accent/10 text-hmi-accent"
              : "border-transparent text-hmi-text hover:bg-hmi-surface-alt"
          }`}
        >
          <button
            type="button"
            onClick={() => selectPlugin(plugin.id)}
            onContextMenu={(event) => {
              event.preventDefault();
              setContextMenu({ pluginId: plugin.id, x: event.clientX, y: event.clientY });
            }}
            className="flex w-full items-center gap-2 px-2 py-1.5 text-left"
          >
            <span className={`h-2 w-2 rounded-full ${stateColor[plugin.state] ?? "bg-hmi-text-muted"}`} />
            <span className="truncate">{plugin.name}</span>
            <span className="ml-auto text-xs text-hmi-text-muted">{plugin.version}</span>
          </button>

          <div className="flex items-center gap-1 px-2 pb-2">
            <button
              type="button"
              onClick={() => {
                if (onTogglePlugin) {
                  void onTogglePlugin(plugin.id);
                  return;
                }
                togglePluginEnabled(plugin.id);
              }}
              className="rounded border border-hmi-border bg-hmi-surface-alt px-2 py-1 text-[11px] text-hmi-text-muted hover:text-hmi-text"
            >
              {plugin.state === "active"
                ? t.pluginTree.disable
                : plugin.state === "inactive"
                  ? t.pluginTree.enable
                  : formatPluginStateLabel(plugin.state, locale)}
            </button>
            <button
              type="button"
              disabled={index === 0}
              onClick={() => movePlugin(plugin.id, plugins[index - 1]!.id)}
              className="rounded border border-hmi-border bg-hmi-surface-alt px-2 py-1 text-[11px] text-hmi-text-muted hover:text-hmi-text disabled:opacity-40"
            >
              {t.pluginTree.up}
            </button>
            <button
              type="button"
              disabled={index === plugins.length - 1}
              onClick={() => movePlugin(plugin.id, plugins[index + 1]!.id)}
              className="rounded border border-hmi-border bg-hmi-surface-alt px-2 py-1 text-[11px] text-hmi-text-muted hover:text-hmi-text disabled:opacity-40"
            >
              {t.pluginTree.down}
            </button>
          </div>
        </div>
      ))}

      {contextMenu && (
        <div
          className="fixed z-30 min-w-32 rounded border border-hmi-border bg-hmi-surface-alt p-1 shadow-lg shadow-black/30"
          style={{ left: contextMenu.x, top: contextMenu.y }}
        >
          {getPluginMenuActions(
            plugins.find((plugin) => plugin.id === contextMenu.pluginId)?.state ?? "inactive",
            locale,
          ).map((action) => (
            <button
              key={action.key}
              type="button"
              className="block w-full rounded px-2 py-1.5 text-left text-xs text-hmi-text hover:bg-hmi-border/40"
              onClick={() => {
                if (action.key === "select") {
                  selectPlugin(contextMenu.pluginId);
                }
                if (action.key === "toggle") {
                  if (onTogglePlugin) {
                    void onTogglePlugin(contextMenu.pluginId);
                  } else {
                    togglePluginEnabled(contextMenu.pluginId);
                  }
                }
                setContextMenu(null);
              }}
            >
              {action.label}
            </button>
          ))}
        </div>
      )}
    </nav>
  );
}
