import { describe, expect, it } from "vitest";

import {
  addEmptyMapping,
  buildPatchConnections,
  buildPatchNodes,
  resolveDropTargetTopic,
  removeMappingAt,
  upsertMappingConnection,
  updateMappingAt,
} from "./topicMapperModel";

describe("addEmptyMapping", () => {
  it("appends a blank mapping row", () => {
    expect(addEmptyMapping([])).toEqual([{ source_topic: "", target_topic: "" }]);
  });
});

describe("updateMappingAt", () => {
  it("updates a mapping field without disturbing other rows", () => {
    const mappings = [
      { source_topic: "a", target_topic: "b" },
      { source_topic: "c", target_topic: "d" },
    ];

    expect(updateMappingAt(mappings, 1, { target_topic: "z" })).toEqual([
      { source_topic: "a", target_topic: "b" },
      { source_topic: "c", target_topic: "z" },
    ]);
  });
});

describe("removeMappingAt", () => {
  it("removes the targeted mapping row", () => {
    const mappings = [
      { source_topic: "a", target_topic: "b" },
      { source_topic: "c", target_topic: "d" },
    ];

    expect(removeMappingAt(mappings, 0)).toEqual([{ source_topic: "c", target_topic: "d" }]);
  });
});

describe("buildPatchNodes", () => {
  it("returns evenly spaced input and output nodes", () => {
    const nodes = buildPatchNodes(
      [
        { id: "in-1", topic: "/in/camera", direction: "input", protocol: "udp", plugin_id: "capture" },
        { id: "in-2", topic: "/in/radar", direction: "input", protocol: "udp", plugin_id: "capture" },
      ],
      [
        { id: "out-1", topic: "/out/camera", direction: "output", protocol: "udp", plugin_id: "writer" },
      ],
    );

    expect(nodes.inputs.map((node) => node.y)).toEqual([24, 52]);
    expect(nodes.outputs.map((node) => node.y)).toEqual([24]);
  });
});

describe("buildPatchConnections", () => {
  it("builds svg-friendly connections for valid mappings", () => {
    const nodes = buildPatchNodes(
      [{ id: "in-1", topic: "/in/camera", direction: "input", protocol: "udp", plugin_id: "capture" }],
      [{ id: "out-1", topic: "/out/camera", direction: "output", protocol: "udp", plugin_id: "writer" }],
    );

    expect(buildPatchConnections(
      [{ source_topic: "/in/camera", target_topic: "/out/camera" }],
      nodes,
    )).toEqual([
      {
        source_topic: "/in/camera",
        target_topic: "/out/camera",
        path: "M 20 24 C 48 24, 72 24, 100 24",
      },
    ]);
  });
});

describe("upsertMappingConnection", () => {
  it("appends a new source-to-target connection", () => {
    expect(upsertMappingConnection([], "/in/camera", "/out/camera")).toEqual([
      { source_topic: "/in/camera", target_topic: "/out/camera" },
    ]);
  });

  it("replaces the target for an existing source topic", () => {
    expect(upsertMappingConnection([
      { source_topic: "/in/camera", target_topic: "/out/old" },
      { source_topic: "/in/radar", target_topic: "/out/radar" },
    ], "/in/camera", "/out/new")).toEqual([
      { source_topic: "/in/camera", target_topic: "/out/new" },
      { source_topic: "/in/radar", target_topic: "/out/radar" },
    ]);
  });
});

describe("resolveDropTargetTopic", () => {
  it("returns the nearest output topic when a drag is released near an output node", () => {
    const nodes = buildPatchNodes(
      [{ id: "in-1", topic: "/in/camera", direction: "input", protocol: "udp", plugin_id: "capture" }],
      [
        { id: "out-1", topic: "/out/camera", direction: "output", protocol: "udp", plugin_id: "writer" },
        { id: "out-2", topic: "/out/radar", direction: "output", protocol: "udp", plugin_id: "writer" },
      ],
    );

    expect(resolveDropTargetTopic(nodes, 26)).toBe("/out/camera");
    expect(resolveDropTargetTopic(nodes, 47)).toBe("/out/radar");
  });

  it("returns null when the pointer is too far away from every output node", () => {
    const nodes = buildPatchNodes(
      [{ id: "in-1", topic: "/in/camera", direction: "input", protocol: "udp", plugin_id: "capture" }],
      [
        { id: "out-1", topic: "/out/camera", direction: "output", protocol: "udp", plugin_id: "writer" },
        { id: "out-2", topic: "/out/radar", direction: "output", protocol: "udp", plugin_id: "writer" },
      ],
    );

    expect(resolveDropTargetTopic(nodes, 120, 18)).toBeNull();
  });

  it("returns null when there are no output nodes to drop onto", () => {
    const nodes = buildPatchNodes(
      [{ id: "in-1", topic: "/in/camera", direction: "input", protocol: "udp", plugin_id: "capture" }],
      [],
    );

    expect(resolveDropTargetTopic(nodes, 24)).toBeNull();
  });
});
