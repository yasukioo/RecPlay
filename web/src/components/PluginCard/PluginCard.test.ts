import { describe, expect, it } from "vitest";

import {
  buildPluginLogEntry,
  getPluginActionLabel,
  getPluginConfigFields,
} from "./pluginCardModel";

describe("getPluginActionLabel", () => {
  it("maps plugin states to primary actions", () => {
    expect(getPluginActionLabel("active")).toBe("Stop Plugin");
    expect(getPluginActionLabel("inactive")).toBe("Start Plugin");
    expect(getPluginActionLabel("error")).toBe("Inspect Fault");
  });
});

describe("getPluginConfigFields", () => {
  it("derives recorder-oriented fields from plugin identity", () => {
    expect(
      getPluginConfigFields({
        id: "recorder.tcp",
        name: "TCP Recorder",
        version: "1.0.0",
        state: "inactive",
        bundle_path: "bundles/tcp.dll",
      }),
    ).toEqual([
      { key: "endpoint", label: "Endpoint", value: "127.0.0.1:9000" },
      { key: "protocol", label: "Protocol", value: "TCP" },
      { key: "buffer", label: "Buffer", value: "4096 frames" },
    ]);
  });

  it("prefers runtime-provided config field metadata over heuristic defaults", () => {
    expect(
      getPluginConfigFields({
        id: "recorder.tcp",
        name: "TCP Recorder",
        version: "1.0.0",
        state: "inactive",
        bundle_path: "bundles/tcp.dll",
        config_fields: [
          { key: "bind", label: "Bind Address", value: "0.0.0.0:9100" },
          { key: "batch", label: "Batch Size", value: "256" },
        ],
      }),
    ).toEqual([
      { key: "bind", label: "Bind Address", value: "0.0.0.0:9100" },
      { key: "batch", label: "Batch Size", value: "256" },
    ]);
  });
});

describe("buildPluginLogEntry", () => {
  it("formats a log line from plugin action context", () => {
    expect(buildPluginLogEntry("capture", "Start Plugin")).toContain("[capture] Start Plugin");
  });
});

describe("localized plugin card model copy", () => {
  it("returns localized actions and config labels", () => {
    expect(getPluginActionLabel("inactive", "zh-CN")).toBe("启动插件");
    expect(
      getPluginConfigFields({
        id: "recorder.generic",
        name: "Recorder",
        version: "1.0.0",
        state: "error",
        bundle_path: "bundles/generic.dll",
      }, "zh-CN"),
    ).toEqual([
      { key: "mode", label: "模式", value: "待机" },
      { key: "profile", label: "配置档", value: "默认" },
      { key: "health", label: "健康状态", value: "故障" },
    ]);
  });
});
