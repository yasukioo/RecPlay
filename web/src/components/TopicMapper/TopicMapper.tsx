import { useMemo, useRef, useState } from "react";

import type { Channel, TopicMapping } from "../../types";
import { getTranslations } from "../../i18n";
import { getMapperClasses } from "../../layoutModel";
import { useLocaleStore } from "../../stores/localeStore";
import {
  addEmptyMapping,
  buildPatchConnections,
  buildPatchNodes,
  createPatchCurve,
  removeMappingAt,
  resolveDropTargetTopic,
  updateMappingAt,
  upsertMappingConnection,
} from "./topicMapperModel";

const PATCH_TARGET_SNAP_DISTANCE = 18;

interface TopicMapperProps {
  channels: Channel[];
  mappings: TopicMapping[];
  onMappingChange?: (mappings: TopicMapping[]) => void;
}

export function TopicMapper({ channels, mappings, onMappingChange }: TopicMapperProps) {
  const locale = useLocaleStore((s) => s.locale);
  const svgRef = useRef<SVGSVGElement | null>(null);
  const [activeSourceTopic, setActiveSourceTopic] = useState<string | null>(null);
  const [previewY, setPreviewY] = useState<number | null>(null);
  const inputChannels = channels.filter((channel) => channel.direction === "input");
  const outputChannels = channels.filter((channel) => channel.direction === "output");
  const layoutClasses = getMapperClasses();
  const t = getTranslations(locale);
  const patchNodes = useMemo(
    () => buildPatchNodes(inputChannels, outputChannels),
    [inputChannels, outputChannels],
  );
  const patchConnections = useMemo(
    () => buildPatchConnections(mappings, patchNodes),
    [mappings, patchNodes],
  );
  const activeSourceNode = patchNodes.inputs.find((node) => node.topic === activeSourceTopic) ?? null;
  const previewPath =
    activeSourceNode && previewY !== null ? createPatchCurve(activeSourceNode.y, previewY) : null;

  const handleAddMapping = () => {
    onMappingChange?.(addEmptyMapping(mappings));
  };

  const handleRemoveMapping = (index: number) => {
    onMappingChange?.(removeMappingAt(mappings, index));
  };

  const updatePreviewFromClientY = (clientY: number) => {
    if (!svgRef.current) {
      return;
    }

    const rect = svgRef.current.getBoundingClientRect();
    if (rect.height <= 0) {
      return;
    }

    const ratio = Math.min(1, Math.max(0, (clientY - rect.top) / rect.height));
    setPreviewY(Math.round(ratio * patchNodes.height));
  };

  const clearPreview = () => {
    setActiveSourceTopic(null);
    setPreviewY(null);
  };

  const commitPreviewConnection = () => {
    if (!activeSourceTopic || previewY === null) {
      clearPreview();
      return;
    }

    const targetTopic = resolveDropTargetTopic(patchNodes, previewY, PATCH_TARGET_SNAP_DISTANCE);
    if (targetTopic) {
      onMappingChange?.(upsertMappingConnection(mappings, activeSourceTopic, targetTopic));
    }
    clearPreview();
  };

  return (
    <div className="space-y-3 rounded border border-hmi-border bg-hmi-surface p-4">
      <div className="flex items-center justify-between">
        <h3 className="text-sm font-bold uppercase tracking-wide text-hmi-accent">
          {t.topicMapper.title}
        </h3>
        <button
          type="button"
          onClick={handleAddMapping}
          className="rounded border border-hmi-accent/30 bg-hmi-accent/10 px-2 py-1 text-xs text-hmi-accent hover:bg-hmi-accent/20"
        >
          {t.topicMapper.add}
        </button>
      </div>

      <div className={layoutClasses.channelGrid}>
        <div>
          <h4 className="mb-1 text-hmi-text-muted">{t.topicMapper.inputs}</h4>
          <div className="flex flex-col gap-2 pt-[14px]">
            {inputChannels.map((channel) => (
              <button
                key={channel.id}
                type="button"
                onPointerDown={() => {
                  setActiveSourceTopic(channel.topic);
                  const node = patchNodes.inputs.find((item) => item.topic === channel.topic);
                  setPreviewY(node?.y ?? null);
                }}
                className={[
                  "truncate rounded border px-2 py-1 text-left font-mono text-xs transition-colors",
                  activeSourceTopic === channel.topic
                    ? "border-hmi-accent/60 bg-hmi-accent/10 text-hmi-accent"
                    : "border-hmi-border bg-hmi-surface-alt text-hmi-text",
                ].join(" ")}
              >
                {channel.topic}
              </button>
            ))}
          </div>
          {inputChannels.length === 0 && (
            <span className="text-hmi-text-muted">{t.topicMapper.noInputs}</span>
          )}
        </div>

        <div
          className="relative hidden items-center justify-center md:flex"
          onPointerMove={(event) => {
            if (!activeSourceTopic) {
              return;
            }
            updatePreviewFromClientY(event.clientY);
          }}
          onPointerUp={() => {
            if (!activeSourceTopic) {
              return;
            }
            commitPreviewConnection();
          }}
          onPointerCancel={clearPreview}
          onPointerLeave={() => {
            if (activeSourceTopic) {
              clearPreview();
            }
          }}
        >
          <svg
            ref={svgRef}
            viewBox={`0 0 120 ${patchNodes.height}`}
            className="w-full"
            style={{ height: `${patchNodes.height}px` }}
          >
            {patchConnections.map((connection) => (
              <g key={`${connection.source_topic}-${connection.target_topic}`}>
                <circle
                  cx="20"
                  cy={patchNodes.inputs.find((node) => node.topic === connection.source_topic)?.y ?? 0}
                  r="4"
                  fill="#4fc3f7"
                />
                <path
                  d={connection.path}
                  stroke="#4fc3f7"
                  strokeWidth="2"
                  fill="none"
                  opacity="0.7"
                />
                <circle
                  cx="100"
                  cy={patchNodes.outputs.find((node) => node.topic === connection.target_topic)?.y ?? 0}
                  r="4"
                  fill="#4fc3f7"
                />
              </g>
            ))}
            {previewPath && (
              <path
                d={previewPath}
                stroke="#ffd166"
                strokeWidth="2"
                strokeDasharray="4 4"
                fill="none"
                opacity="0.85"
              />
            )}
          </svg>
        </div>

        <div>
          <h4 className="mb-1 text-hmi-text-muted">{t.topicMapper.outputs}</h4>
          <div className="flex flex-col gap-2 pt-[14px]">
            {outputChannels.map((channel) => (
              <button
                key={channel.id}
                type="button"
                onPointerEnter={(event) => {
                  if (!activeSourceTopic) {
                    return;
                  }
                  updatePreviewFromClientY(event.clientY);
                }}
                onClick={() => {
                  if (!activeSourceTopic) {
                    return;
                  }

                  onMappingChange?.(upsertMappingConnection(mappings, activeSourceTopic, channel.topic));
                  clearPreview();
                }}
                className="truncate rounded border border-hmi-border bg-hmi-surface-alt px-2 py-1 text-left font-mono text-xs text-hmi-text transition-colors hover:border-hmi-accent/40 hover:text-hmi-accent"
              >
                {channel.topic}
              </button>
            ))}
          </div>
          {outputChannels.length === 0 && (
            <span className="text-hmi-text-muted">{t.topicMapper.noOutputs}</span>
          )}
        </div>
      </div>

      <div className="space-y-2">
        {mappings.map((mapping, index) => (
          <div key={index} className={layoutClasses.mappingRow}>
            <select
              value={mapping.source_topic}
              onChange={(event) =>
                onMappingChange?.(
                  updateMappingAt(mappings, index, { source_topic: event.target.value }),
                )
              }
              className="rounded border border-hmi-border bg-hmi-bg px-2 py-1 text-xs text-hmi-text"
            >
              <option value="">{t.topicMapper.selectInput}</option>
              {inputChannels.map((channel) => (
                <option key={channel.id} value={channel.topic}>
                  {channel.topic}
                </option>
              ))}
            </select>
            <select
              value={mapping.target_topic}
              onChange={(event) =>
                onMappingChange?.(
                  updateMappingAt(mappings, index, { target_topic: event.target.value }),
                )
              }
              className="rounded border border-hmi-border bg-hmi-bg px-2 py-1 text-xs text-hmi-text"
            >
              <option value="">{t.topicMapper.selectOutput}</option>
              {outputChannels.map((channel) => (
                <option key={channel.id} value={channel.topic}>
                  {channel.topic}
                </option>
              ))}
            </select>
            <button
              type="button"
              onClick={() => handleRemoveMapping(index)}
              className="rounded border border-hmi-danger/30 bg-hmi-danger/10 px-2 py-1 text-xs text-hmi-danger hover:bg-hmi-danger/20"
            >
              {t.topicMapper.remove}
            </button>
          </div>
        ))}

        {mappings.length === 0 && (
          <span className="text-xs text-hmi-text-muted">{t.topicMapper.noMappings}</span>
        )}
      </div>
    </div>
  );
}
