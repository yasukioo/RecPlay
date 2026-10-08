import { afterEach, describe, expect, it, vi } from "vitest";

import {
  getChannels,
  getPluginDetail,
  getPlugins,
  getTopicMappings,
  openPlaybackFile,
  pausePlayback,
  pauseRecording,
  playPlayback,
  resumeRecording,
  seekPlayback,
  setPluginConfig,
  setPlaybackLoop,
  setPlaybackSpeed,
  setTopicMappings,
  startPlugin,
  startRecording,
  stopPlugin,
  stopPlayback,
  stopRecording,
} from "./client";

describe("api client command routes", () => {
  afterEach(() => {
    vi.restoreAllMocks();
  });

  it("posts recording and playback commands with expected payloads", async () => {
    const fetchMock = vi.fn().mockResolvedValue({
      ok: true,
      json: async () => ({ ok: true }),
    });

    vi.stubGlobal("fetch", fetchMock);

    await startRecording({ output_path: "capture.rpcap", protocols: ["UDP"] });
    await pauseRecording();
    await resumeRecording();
    await stopRecording();
    await openPlaybackFile("capture.rpcap");
    await playPlayback(1.5);
    await pausePlayback();
    await seekPlayback(400);
    await setPlaybackSpeed(2.0);
    await setPlaybackLoop(100, 300);
    await stopPlayback();

    expect(fetchMock).toHaveBeenNthCalledWith(
      1,
      "/api/session/record",
      expect.objectContaining({
        method: "POST",
        body: JSON.stringify({ output_path: "capture.rpcap", protocols: ["UDP"] }),
      }),
    );
    expect(fetchMock).toHaveBeenNthCalledWith(
      5,
      "/api/session/playback/open",
      expect.objectContaining({
        method: "POST",
        body: JSON.stringify({ file_path: "capture.rpcap" }),
      }),
    );
    expect(fetchMock).toHaveBeenNthCalledWith(
      6,
      "/api/session/playback/play",
      expect.objectContaining({
        method: "POST",
        body: JSON.stringify({ speed: 1.5 }),
      }),
    );
    expect(fetchMock).toHaveBeenNthCalledWith(
      8,
      "/api/session/playback/seek",
      expect.objectContaining({
        method: "POST",
        body: JSON.stringify({ timestamp_ns: 400 }),
      }),
    );
    expect(fetchMock).toHaveBeenNthCalledWith(
      10,
      "/api/session/playback/loop",
      expect.objectContaining({
        method: "POST",
        body: JSON.stringify({ start_ns: 100, end_ns: 300 }),
      }),
    );
    expect(fetchMock).toHaveBeenNthCalledWith(
      11,
      "/api/session/playback/stop",
      expect.objectContaining({
        method: "POST",
      }),
    );
  });

  it("calls plugin, channel, and mapping routes with the expected payloads", async () => {
    const fetchMock = vi.fn().mockResolvedValue({
      ok: true,
      json: async () => ({ ok: true }),
    });

    vi.stubGlobal("fetch", fetchMock);

    await getPlugins();
    await getPluginDetail("UDP");
    await setPluginConfig("UDP", [
      { key: "address", label: "Address", value: "239.9.9.9" },
      { key: "port", label: "Port", value: "5500" },
    ]);
    await startPlugin("UDP");
    await stopPlugin("UDP");
    await getChannels();
    await getTopicMappings();
    await setTopicMappings([{ source_topic: "/in/radar", target_topic: "/out/radar" }]);

    expect(fetchMock).toHaveBeenNthCalledWith(1, "/api/plugins", expect.any(Object));
    expect(fetchMock).toHaveBeenNthCalledWith(2, "/api/plugins/UDP", expect.any(Object));
    expect(fetchMock).toHaveBeenNthCalledWith(
      3,
      "/api/plugins/UDP/config",
      expect.objectContaining({
        method: "POST",
        body: JSON.stringify({
          config_fields: [
            { key: "address", label: "Address", value: "239.9.9.9" },
            { key: "port", label: "Port", value: "5500" },
          ],
        }),
      }),
    );
    expect(fetchMock).toHaveBeenNthCalledWith(
      4,
      "/api/plugins/UDP/start",
      expect.objectContaining({ method: "POST" }),
    );
    expect(fetchMock).toHaveBeenNthCalledWith(
      5,
      "/api/plugins/UDP/stop",
      expect.objectContaining({ method: "POST" }),
    );
    expect(fetchMock).toHaveBeenNthCalledWith(6, "/api/channels", expect.any(Object));
    expect(fetchMock).toHaveBeenNthCalledWith(7, "/api/mappings", expect.any(Object));
    expect(fetchMock).toHaveBeenNthCalledWith(
      8,
      "/api/mappings",
      expect.objectContaining({
        method: "POST",
        body: JSON.stringify({
          mappings: [{ source_topic: "/in/radar", target_topic: "/out/radar" }],
        }),
      }),
    );
  });
});
