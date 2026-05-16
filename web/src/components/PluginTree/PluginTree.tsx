import { usePluginStore } from "../../stores/pluginStore";

export function PluginTree() {
  const plugins = usePluginStore((s) => s.plugins);
  const selectedId = usePluginStore((s) => s.selectedPluginId);
  const selectPlugin = usePluginStore((s) => s.selectPlugin);

  const stateColor: Record<string, string> = {
    active: "bg-hmi-success",
    inactive: "bg-hmi-text-muted",
    error: "bg-hmi-danger",
  };

  // TODO: Codex — implement drag-to-reorder, context menu, plugin enable/disable toggle

  return (
    <nav className="flex flex-col gap-1 p-2 bg-hmi-surface rounded border border-hmi-border overflow-y-auto">
      <h3 className="text-xs font-bold text-hmi-text-muted uppercase tracking-wide px-2 py-1">
        Plugins
      </h3>

      {plugins.length === 0 && (
        <span className="text-xs text-hmi-text-muted px-2 py-4 text-center">
          No plugins loaded
        </span>
      )}

      {plugins.map((plugin) => (
        <button
          key={plugin.id}
          type="button"
          onClick={() => selectPlugin(plugin.id)}
          className={`flex items-center gap-2 px-2 py-1.5 rounded text-left text-sm transition-colors ${
            selectedId === plugin.id
              ? "bg-hmi-accent/10 text-hmi-accent border border-hmi-accent/30"
              : "hover:bg-hmi-surface-alt text-hmi-text border border-transparent"
          }`}
        >
          <span className={`w-2 h-2 rounded-full ${stateColor[plugin.state] ?? "bg-hmi-text-muted"}`} />
          <span className="truncate">{plugin.name}</span>
          <span className="ml-auto text-xs text-hmi-text-muted">{plugin.version}</span>
        </button>
      ))}
    </nav>
  );
}
