import type { Channel, TopicMapping } from "../../types";

interface TopicMapperProps {
  channels: Channel[];
  mappings: TopicMapping[];
  onMappingChange?: (mappings: TopicMapping[]) => void;
}

export function TopicMapper({ channels, mappings, onMappingChange }: TopicMapperProps) {
  // TODO: Codex — implement drag-and-drop patch bay UI, connection lines (SVG/Canvas)

  const handleAddMapping = () => {
    console.log("TopicMapper: add mapping");
    onMappingChange?.([...mappings, { source_topic: "", target_topic: "" }]);
  };

  const handleRemoveMapping = (index: number) => {
    console.log("TopicMapper: remove mapping", index);
    onMappingChange?.(mappings.filter((_, i) => i !== index));
  };

  return (
    <div className="p-4 bg-hmi-surface rounded border border-hmi-border space-y-3">
      <div className="flex items-center justify-between">
        <h3 className="text-sm font-bold text-hmi-accent uppercase tracking-wide">
          Topic Mapper (Patch Bay)
        </h3>
        <button
          type="button"
          onClick={handleAddMapping}
          className="text-xs px-2 py-1 rounded bg-hmi-accent/10 text-hmi-accent border border-hmi-accent/30 hover:bg-hmi-accent/20"
        >
          + Add
        </button>
      </div>

      {/* Channel list */}
      <div className="grid grid-cols-2 gap-4 text-xs">
        <div>
          <h4 className="text-hmi-text-muted mb-1">Inputs</h4>
          {channels.filter((c) => c.direction === "input").map((c) => (
            <div key={c.id} className="py-0.5 font-mono truncate">{c.topic}</div>
          ))}
          {channels.filter((c) => c.direction === "input").length === 0 && (
            <span className="text-hmi-text-muted">No input channels</span>
          )}
        </div>
        <div>
          <h4 className="text-hmi-text-muted mb-1">Outputs</h4>
          {channels.filter((c) => c.direction === "output").map((c) => (
            <div key={c.id} className="py-0.5 font-mono truncate">{c.topic}</div>
          ))}
          {channels.filter((c) => c.direction === "output").length === 0 && (
            <span className="text-hmi-text-muted">No output channels</span>
          )}
        </div>
      </div>

      {/* Mapping table */}
      <div className="space-y-1">
        {mappings.map((m, i) => (
          <div key={i} className="flex items-center gap-2 text-xs bg-hmi-surface-alt rounded px-2 py-1">
            <span className="font-mono truncate flex-1">{m.source_topic || "—"}</span>
            <span className="text-hmi-accent">→</span>
            <span className="font-mono truncate flex-1">{m.target_topic || "—"}</span>
            <button
              type="button"
              onClick={() => handleRemoveMapping(i)}
              className="text-hmi-danger hover:text-hmi-danger/80"
            >
              ×
            </button>
          </div>
        ))}
        {mappings.length === 0 && (
          <span className="text-xs text-hmi-text-muted">No mappings configured</span>
        )}
      </div>
    </div>
  );
}
