import type { PluginInfo } from "../../types";

interface PluginCardProps {
  plugin: PluginInfo | null;
}

export function PluginCard({ plugin }: PluginCardProps) {
  if (!plugin) {
    return (
      <div className="flex items-center justify-center h-full text-hmi-text-muted text-sm">
        Select a plugin from the tree
      </div>
    );
  }

  // TODO: Codex — implement plugin configuration form, start/stop controls, log viewer

  return (
    <div className="p-4 bg-hmi-surface rounded border border-hmi-border space-y-3">
      <div className="flex items-center justify-between">
        <h3 className="font-bold text-hmi-text">{plugin.name}</h3>
        <span className={`text-xs px-2 py-0.5 rounded ${
          plugin.state === "active"
            ? "bg-hmi-success/20 text-hmi-success"
            : plugin.state === "error"
              ? "bg-hmi-danger/20 text-hmi-danger"
              : "bg-hmi-surface-alt text-hmi-text-muted"
        }`}>
          {plugin.state}
        </span>
      </div>

      <dl className="text-sm space-y-1">
        <div className="flex gap-2">
          <dt className="text-hmi-text-muted">ID:</dt>
          <dd className="font-mono text-xs">{plugin.id}</dd>
        </div>
        <div className="flex gap-2">
          <dt className="text-hmi-text-muted">Version:</dt>
          <dd>{plugin.version}</dd>
        </div>
        <div className="flex gap-2">
          <dt className="text-hmi-text-muted">Path:</dt>
          <dd className="font-mono text-xs truncate">{plugin.bundle_path}</dd>
        </div>
      </dl>

      {/* TODO: Codex — plugin-specific configuration form fields here */}
      <div className="border-t border-hmi-border pt-3">
        <p className="text-xs text-hmi-text-muted">Configuration panel placeholder</p>
      </div>
    </div>
  );
}
