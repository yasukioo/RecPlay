import type { SessionStateSnapshot, StatsSnapshot, PluginInfo, TopicMapping, Channel } from "../types";

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

// --- Session API ---

export function getSessionState(): Promise<Omit<SessionStateSnapshot, "lastTransition">> {
  return requestJson("/api/session/state");
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

export function getPlugins(): Promise<PluginInfo[]> {
  return requestJson("/api/plugins");
}

export function getPluginDetail(id: string): Promise<PluginInfo> {
  return requestJson(`/api/plugins/${id}`);
}

export function setPluginConfig(
  id: string,
  configFields: NonNullable<PluginInfo["config_fields"]>,
): Promise<PluginInfo> {
  return postJson(`/api/plugins/${id}/config`, { config_fields: configFields });
}

export function startPlugin(id: string): Promise<PluginInfo> {
  return postJson(`/api/plugins/${id}/start`);
}

export function stopPlugin(id: string): Promise<PluginInfo> {
  return postJson(`/api/plugins/${id}/stop`);
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
  return requestJson("/api/channels");
}

// --- File listing API ---

export function listRecordingFiles(): Promise<{ files: string[] }> {
  return requestJson("/api/files");
}

// --- WebSocket URL ---

export function getWebSocketUrl(path = "/api/ws"): string {
  const protocol = window.location.protocol === "https:" ? "wss:" : "ws:";
  return `${protocol}//${window.location.host}${path}`;
}
