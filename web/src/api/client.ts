import type {
  Channel,
  DensityResponse,
  LogEntry,
  PacketInspect,
  PluginInfo,
  RecordingFile,
  ReplayTarget,
  SessionStateSnapshot,
  StatsSnapshot,
  TimelineMarker,
  TopicMapping,
} from "../types";
import { buildPluginConfigMap, inferPluginKind, inferProtocol } from "../views/workbenchModel";

async function requestJson<TResponse>(path: string, init?: RequestInit): Promise<TResponse> {
  const response = await fetch(path, {
    ...init,
    headers: {
      "Content-Type": "application/json",
      ...(init?.headers ?? {}),
    },
  });

  if (!response.ok) {
    throw new Error(`${init?.method ?? "GET"} ${path} failed with ${response.status}`);
  }

  return (await response.json()) as TResponse;
}

async function postJson<TResponse>(path: string, body?: unknown): Promise<TResponse> {
  return requestJson<TResponse>(path, {
    method: "POST",
    body: body === undefined ? undefined : JSON.stringify(body),
  });
}

async function requestOptionalJson<TResponse>(path: string, fallback: TResponse, init?: RequestInit): Promise<TResponse> {
  try {
    return await requestJson<TResponse>(path, init);
  } catch {
    return fallback;
  }
}

// --- Session API ---

export function getSessionState(): Promise<Omit<SessionStateSnapshot, "lastTransition">> {
  return requestJson("/api/session/state");
}

export function resetSession(): Promise<{ ok: boolean }> {
  return postJson("/api/session/reset");
}

export function startRecording(config: Record<string, unknown>): Promise<{ ok: boolean }> {
  return postJson("/api/session/record", config);
}

export function pauseRecording(): Promise<{ ok: boolean }> {
  return postJson("/api/session/record/pause");
}

export function resumeRecording(): Promise<{ ok: boolean }> {
  return postJson("/api/session/record/resume");
}

export function stopRecording(): Promise<{ ok: boolean }> {
  return postJson("/api/session/record/stop");
}

export function openPlaybackFile(filePath: string): Promise<{ ok: boolean }> {
  return postJson("/api/session/playback/open", { file_path: filePath });
}

export function playPlayback(speed = 1): Promise<{ ok: boolean }> {
  return postJson("/api/session/playback/play", { speed });
}

export function pausePlayback(): Promise<{ ok: boolean }> {
  return postJson("/api/session/playback/pause");
}

export function seekPlayback(timestampNs: number): Promise<{ ok: boolean }> {
  return postJson("/api/session/playback/seek", { timestamp_ns: timestampNs });
}

export function setPlaybackSpeed(speed: number): Promise<{ ok: boolean }> {
  return postJson("/api/session/playback/speed", { speed });
}

export function setPlaybackLoop(startNs: number, endNs: number): Promise<{ ok: boolean }> {
  return postJson("/api/session/playback/loop", { start_ns: startNs, end_ns: endNs });
}

export function stopPlayback(): Promise<{ ok: boolean }> {
  return postJson("/api/session/playback/stop");
}

// --- Stats API ---

export function getStatsSnapshot(): Promise<StatsSnapshot> {
  return requestJson("/api/stats");
}

// --- Plugin API ---

export async function getPlugins(): Promise<PluginInfo[]> {
  const plugins = await requestJson<Array<Record<string, unknown>> | Record<string, unknown>>("/api/plugins");
  return Array.isArray(plugins) ? plugins.map(normalizePlugin) : [];
}

export async function getPluginDetail(id: string): Promise<PluginInfo> {
  return normalizePlugin(await requestJson<Record<string, unknown>>(`/api/plugins/${id}`));
}

export function setPluginConfig(
  id: string,
  configFields: NonNullable<PluginInfo["config_fields"]>,
): Promise<PluginInfo> {
  return postJson<Record<string, unknown>>(`/api/plugins/${id}/config`, { config_fields: configFields }).then(normalizePlugin);
}

export function startPlugin(id: string): Promise<PluginInfo> {
  return postJson<Record<string, unknown>>(`/api/plugins/${id}/start`).then(normalizePlugin);
}

export function stopPlugin(id: string): Promise<PluginInfo> {
  return postJson<Record<string, unknown>>(`/api/plugins/${id}/stop`).then(normalizePlugin);
}

// --- Topic Mapping API ---

export function getTopicMappings(): Promise<TopicMapping[]> {
  return requestJson("/api/mappings");
}

export function setTopicMappings(mappings: TopicMapping[]): Promise<{ ok: boolean }> {
  return postJson("/api/mappings", { mappings });
}

// --- Channel API ---

export function getChannels(): Promise<Channel[]> {
  return requestJson<Array<Record<string, unknown>> | Record<string, unknown>>("/api/channels").then((channels) =>
    Array.isArray(channels) ? channels.map(normalizeChannel) : [],
  );
}

// --- File listing API ---

export async function listRecordingFiles(): Promise<RecordingFile[]> {
  const response = await requestOptionalJson<Record<string, unknown> | Array<Record<string, unknown>>>(
    "/api/files",
    { files: [] },
  );
  return normalizeRecordingFiles(response);
}

export async function getMarkers(): Promise<TimelineMarker[]> {
  const response = await requestOptionalJson<Record<string, unknown> | TimelineMarker[]>("/api/markers", { markers: [] });
  if (Array.isArray(response)) {
    return response;
  }
  const markers = Array.isArray(response.markers) ? response.markers : [];
  return markers.map(normalizeMarker);
}

export async function getPlaybackDensity(): Promise<DensityResponse> {
  const response = await requestOptionalJson<Record<string, unknown>>("/api/playback/density", {
    duration_ns: 0,
    buckets: 0,
    total: [],
  });
  const total = Array.isArray(response.total)
    ? response.total.filter((value): value is number => typeof value === "number" && Number.isFinite(value))
    : [];
  return {
    duration_ns: typeof response.duration_ns === "number" ? response.duration_ns : 0,
    buckets: typeof response.buckets === "number" ? response.buckets : total.length,
    total,
  };
}

export async function getCurrentPacket(): Promise<PacketInspect | null> {
  const response = await requestOptionalJson<Record<string, unknown> | null>("/api/playback/packet", null);
  return response ? normalizePacketInspect(response) : null;
}

export async function getReplayTargets(): Promise<ReplayTarget[]> {
  const response = await requestOptionalJson<Record<string, unknown> | ReplayTarget[]>("/api/playback/targets", {
    targets: [],
  });
  if (Array.isArray(response)) {
    return response;
  }
  const targets = Array.isArray(response.targets) ? response.targets : [];
  return targets.map(normalizeReplayTarget);
}

export async function setReplayTargets(targets: ReplayTarget[]): Promise<ReplayTarget[]> {
  const response = await requestOptionalJson<Record<string, unknown> | ReplayTarget[]>(
    "/api/playback/targets",
    targets,
    {
      method: "POST",
      headers: { "Content-Type": "application/json" },
      body: JSON.stringify({ targets }),
    },
  );
  if (Array.isArray(response)) {
    return response;
  }
  const nextTargets = Array.isArray(response.targets) ? response.targets : targets;
  return nextTargets.map(normalizeReplayTarget);
}

export async function getEventLog(): Promise<LogEntry[]> {
  const response = await requestOptionalJson<Record<string, unknown> | LogEntry[]>("/api/logs", { entries: [] });
  if (Array.isArray(response)) {
    return response;
  }
  const entries = Array.isArray(response.entries) ? response.entries : [];
  return entries.map(normalizeLogEntry);
}

// --- WebSocket URL ---

export function getWebSocketUrl(path = "/api/ws"): string {
  const protocol = window.location.protocol === "https:" ? "wss:" : "ws:";
  return `${protocol}//${window.location.host}${path}`;
}

function normalizePlugin(plugin: Record<string, unknown>): PluginInfo {
  const configFields = Array.isArray(plugin.config_fields)
    ? plugin.config_fields
        .filter((value): value is Record<string, unknown> => Boolean(value) && typeof value === "object")
        .map((field) => ({
          key: String(field.key ?? ""),
          label: String(field.label ?? field.key ?? ""),
          value: String(field.value ?? ""),
        }))
    : [];

  const base: PluginInfo = {
    id: String(plugin.id ?? ""),
    name: String(plugin.name ?? plugin.id ?? "Plugin"),
    version: String(plugin.version ?? "—"),
    state: normalizePluginState(plugin.state),
    bundle_path: String(plugin.bundle_path ?? ""),
    kind: normalizePluginKind(plugin.kind),
    protocol: typeof plugin.protocol === "string" ? plugin.protocol : undefined,
    desc: typeof plugin.desc === "string" ? plugin.desc : undefined,
    config_fields: configFields,
  };

  const kind = base.kind ?? inferPluginKind(base);
  const protocol = base.protocol ?? inferProtocol(base);

  return {
    ...base,
    kind,
    protocol,
    desc: base.desc ?? [kind, protocol].filter(Boolean).join(" · "),
    config: buildPluginConfigMap({ ...base, config_fields: configFields }),
  };
}

function normalizePluginState(value: unknown): PluginInfo["state"] {
  return value === "active" || value === "error" ? value : "inactive";
}

// Accept either the capitalized form ("Source") the frontend expects
// or the lowercase form ("source") the C++ backend currently returns.
function normalizePluginKind(value: unknown): PluginInfo["kind"] | undefined {
  if (typeof value !== "string" || value === "") return undefined;
  const cap = (value.charAt(0).toUpperCase() + value.slice(1)) as PluginInfo["kind"];
  return isPluginKind(cap) ? cap : undefined;
}

function normalizeChannel(channel: Record<string, unknown>): Channel {
  return {
    id: String(channel.id ?? ""),
    name: typeof channel.name === "string" ? channel.name : undefined,
    topic: String(channel.topic ?? ""),
    direction: channel.direction === "output" ? "output" : "input",
    protocol: String(channel.protocol ?? "—"),
    plugin_id: String(channel.plugin_id ?? ""),
    rate: typeof channel.rate === "number" ? channel.rate : undefined,
    bytes: typeof channel.bytes === "number" ? channel.bytes : undefined,
    drop: typeof channel.drop === "number" ? channel.drop : undefined,
    color: typeof channel.color === "string" ? channel.color : undefined,
    enabled: typeof channel.enabled === "boolean" ? channel.enabled : true,
  };
}

function normalizeRecordingFiles(response: Record<string, unknown> | Array<Record<string, unknown>>): RecordingFile[] {
  if (Array.isArray(response)) {
    return response.map(normalizeRecordingFile);
  }

  const files = Array.isArray(response.files) ? response.files : [];
  if (files.every((file) => typeof file === "string")) {
    return files.map((name) => ({
      name,
      path: name,
      size: null,
      duration_ns: null,
      channels: null,
      recorded_at: null,
      tag: null,
      mock: true,
    }));
  }

  return files
    .filter((file): file is Record<string, unknown> => Boolean(file) && typeof file === "object")
    .map(normalizeRecordingFile);
}

function normalizeRecordingFile(file: Record<string, unknown>): RecordingFile {
  return {
    name: String(file.name ?? file.file ?? ""),
    path: typeof file.path === "string" ? file.path : typeof file.file === "string" ? file.file : null,
    size: normalizeNullableNumber(file.size),
    duration_ns: normalizeNullableNumber(file.duration_ns ?? file.dur),
    channels: normalizeNullableNumber(file.channels ?? file.chans),
    recorded_at: normalizeNullableNumber(file.recorded_at ?? file.ts),
    tag: typeof file.tag === "string" ? file.tag : null,
    codec: typeof file.codec === "string" ? file.codec : null,
    checksum: typeof file.checksum === "string" ? file.checksum : null,
    mock: false,
  };
}

function normalizeMarker(marker: Record<string, unknown>): TimelineMarker {
  return {
    timestamp_ns: Number(marker.timestamp_ns ?? marker.t ?? 0),
    label: String(marker.label ?? marker.name ?? "marker"),
    type:
      marker.type === "bookmark" || marker.type === "error" || marker.type === "event"
        ? marker.type
        : "event",
    color: typeof marker.color === "string" ? marker.color : undefined,
  };
}

function normalizePacketInspect(packet: Record<string, unknown>): PacketInspect {
  return {
    channel: String(packet.channel ?? "—"),
    plugin: String(packet.plugin ?? "—"),
    seq: Number(packet.seq ?? 0),
    t_record: Number(packet.t_record ?? 0),
    t_replay: Number(packet.t_replay ?? 0),
    size: Number(packet.size ?? 0),
    topic: String(packet.topic ?? "—"),
    writer: String(packet.writer ?? packet.protocol ?? "—"),
    hex: typeof packet.hex === "string" ? packet.hex : "",
    metadata:
      packet.metadata && typeof packet.metadata === "object"
        ? Object.fromEntries(Object.entries(packet.metadata as Record<string, unknown>).map(([key, value]) => [key, String(value)]))
        : undefined,
    mock: false,
  };
}

function normalizeReplayTarget(target: Record<string, unknown>): ReplayTarget {
  return {
    id: String(target.id ?? target.protocol ?? target.name ?? ""),
    name: String(target.name ?? target.id ?? "target"),
    protocol: String(target.protocol ?? "—"),
    enabled: Boolean(target.enabled),
    endpoint: typeof target.endpoint === "string" ? target.endpoint : undefined,
    status:
      target.status === "active" || target.status === "error" || target.status === "idle"
        ? target.status
        : undefined,
    mock: false,
  };
}

function normalizeLogEntry(entry: Record<string, unknown>): LogEntry {
  const ts = Number(entry.ts ?? Date.now());
  const level = entry.level === "ok" || entry.level === "warn" || entry.level === "err" ? entry.level : "info";
  return {
    id: String(entry.id ?? `${ts}-${Math.random().toString(16).slice(2, 8)}`),
    ts,
    level,
    message: String(entry.message ?? entry.msg ?? ""),
    source: typeof entry.source === "string" ? entry.source : undefined,
    mock: false,
  };
}

function normalizeNullableNumber(value: unknown): number | null {
  return typeof value === "number" && Number.isFinite(value) ? value : null;
}

function isPluginKind(value: unknown): value is PluginInfo["kind"] {
  return value === "Source" || value === "Sink" || value === "Codec" || value === "Service";
}
