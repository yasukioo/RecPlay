import type { Channel, TopicMapping } from "../../types";

export interface PatchNode {
  topic: string;
  x: number;
  y: number;
}

export interface PatchNodeSet {
  inputs: PatchNode[];
  outputs: PatchNode[];
  height: number;
}

export interface PatchConnection {
  source_topic: string;
  target_topic: string;
  path: string;
}

const PATCH_LEFT_X = 20;
const PATCH_RIGHT_X = 100;
const PATCH_TOP_Y = 24;
const PATCH_ROW_GAP = 28;

export function addEmptyMapping(mappings: TopicMapping[]): TopicMapping[] {
  return [...mappings, { source_topic: "", target_topic: "" }];
}

export function updateMappingAt(
  mappings: TopicMapping[],
  index: number,
  patch: Partial<TopicMapping>,
): TopicMapping[] {
  return mappings.map((mapping, mappingIndex) =>
    mappingIndex === index ? { ...mapping, ...patch } : mapping,
  );
}

export function removeMappingAt(mappings: TopicMapping[], index: number): TopicMapping[] {
  return mappings.filter((_, mappingIndex) => mappingIndex !== index);
}

export function buildPatchNodes(inputChannels: Channel[], outputChannels: Channel[]): PatchNodeSet {
  const inputs = inputChannels.map((channel, index) => ({
    topic: channel.topic,
    x: PATCH_LEFT_X,
    y: PATCH_TOP_Y + (index * PATCH_ROW_GAP),
  }));
  const outputs = outputChannels.map((channel, index) => ({
    topic: channel.topic,
    x: PATCH_RIGHT_X,
    y: PATCH_TOP_Y + (index * PATCH_ROW_GAP),
  }));
  const maxCount = Math.max(inputs.length, outputs.length, 1);

  return {
    inputs,
    outputs,
    height: PATCH_TOP_Y + ((maxCount - 1) * PATCH_ROW_GAP) + PATCH_TOP_Y,
  };
}

export function buildPatchConnections(
  mappings: TopicMapping[],
  nodes: PatchNodeSet,
): PatchConnection[] {
  return mappings.flatMap((mapping) => {
    const source = nodes.inputs.find((node) => node.topic === mapping.source_topic);
    const target = nodes.outputs.find((node) => node.topic === mapping.target_topic);
    if (!source || !target) {
      return [];
    }

    return [{
      source_topic: mapping.source_topic,
      target_topic: mapping.target_topic,
      path: createPatchCurve(source.y, target.y),
    }];
  });
}

export function upsertMappingConnection(
  mappings: TopicMapping[],
  sourceTopic: string,
  targetTopic: string,
): TopicMapping[] {
  if (!sourceTopic || !targetTopic) {
    return mappings;
  }

  const nextMapping = { source_topic: sourceTopic, target_topic: targetTopic };
  const existingIndex = mappings.findIndex((mapping) => mapping.source_topic === sourceTopic);
  if (existingIndex === -1) {
    return [...mappings, nextMapping];
  }

  return mappings.map((mapping, index) => (index === existingIndex ? nextMapping : mapping));
}

export function resolveDropTargetTopic(
  nodes: PatchNodeSet,
  previewY: number,
  maxDistance = Number.POSITIVE_INFINITY,
): string | null {
  if (nodes.outputs.length === 0) {
    return null;
  }

  let nearest = nodes.outputs[0] ?? null;
  let nearestDistance = nearest ? Math.abs(nearest.y - previewY) : Number.POSITIVE_INFINITY;

  for (const output of nodes.outputs) {
    const distance = Math.abs(output.y - previewY);
    if (distance < nearestDistance) {
      nearest = output;
      nearestDistance = distance;
    }
  }

  if (nearestDistance > maxDistance) {
    return null;
  }

  return nearest?.topic ?? null;
}

export function createPatchCurve(sourceY: number, targetY: number): string {
  return `M ${PATCH_LEFT_X} ${sourceY} C 48 ${sourceY}, 72 ${targetY}, ${PATCH_RIGHT_X} ${targetY}`;
}
