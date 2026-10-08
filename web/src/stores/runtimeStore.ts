import { create } from "zustand";

import type {
  Channel,
  DensityResponse,
  LogEntry,
  PacketInspect,
  RecordingFile,
  ReplayTarget,
  TimelineMarker,
} from "../types";

const MAX_LOG_ENTRIES = 200;

const EMPTY_DENSITY: DensityResponse = { duration_ns: 0, buckets: 0, total: [] };

interface RuntimeStoreState {
  channels: Channel[];
  recordingFiles: RecordingFile[];
  markers: TimelineMarker[];
  replayTargets: ReplayTarget[];
  currentPacket: PacketInspect | null;
  eventLog: LogEntry[];
  density: DensityResponse;
  selectedFileName: string | null;
  selectedChannelId: string | null;
  setChannels: (channels: Channel[]) => void;
  updateChannel: (channelId: string, patch: Partial<Channel>) => void;
  toggleChannelEnabled: (channelId: string) => void;
  setRecordingFiles: (files: RecordingFile[]) => void;
  selectFile: (fileName: string | null) => void;
  setMarkers: (markers: TimelineMarker[]) => void;
  setReplayTargets: (targets: ReplayTarget[]) => void;
  toggleReplayTarget: (targetId: string) => void;
  setCurrentPacket: (packet: PacketInspect | null) => void;
  replaceEventLog: (entries: LogEntry[]) => void;
  appendEventLog: (entry: LogEntry) => void;
  setDensity: (density: DensityResponse) => void;
  selectChannel: (channelId: string | null) => void;
}

export const useRuntimeStore = create<RuntimeStoreState>((set) => ({
  channels: [],
  recordingFiles: [],
  markers: [],
  replayTargets: [],
  currentPacket: null,
  eventLog: [],
  density: EMPTY_DENSITY,
  selectedFileName: null,
  selectedChannelId: null,

  setChannels: (channels) =>
    set((state) => {
      const selectedChannelId =
        state.selectedChannelId && channels.some((channel) => channel.id === state.selectedChannelId)
          ? state.selectedChannelId
          : channels[0]?.id ?? null;

      return { channels, selectedChannelId };
    }),

  updateChannel: (channelId, patch) =>
    set((state) => ({
      channels: state.channels.map((channel) =>
        channel.id === channelId ? { ...channel, ...patch } : channel,
      ),
    })),

  toggleChannelEnabled: (channelId) =>
    set((state) => ({
      channels: state.channels.map((channel) =>
        channel.id === channelId
          ? { ...channel, enabled: channel.enabled === false ? true : false }
          : channel,
      ),
    })),

  setRecordingFiles: (recordingFiles) =>
    set((state) => {
      const selectedFileName =
        state.selectedFileName &&
        recordingFiles.some((file) => file.name === state.selectedFileName)
          ? state.selectedFileName
          : recordingFiles[0]?.name ?? null;

      return { recordingFiles, selectedFileName };
    }),

  selectFile: (selectedFileName) => set({ selectedFileName }),

  setMarkers: (markers) => set({ markers }),

  setReplayTargets: (replayTargets) => set({ replayTargets }),

  toggleReplayTarget: (targetId) =>
    set((state) => ({
      replayTargets: state.replayTargets.map((target) =>
        target.id === targetId ? { ...target, enabled: !target.enabled } : target,
      ),
    })),

  setCurrentPacket: (currentPacket) => set({ currentPacket }),

  replaceEventLog: (eventLog) => set({ eventLog: eventLog.slice(0, MAX_LOG_ENTRIES) }),

  appendEventLog: (entry) =>
    set((state) => ({
      eventLog: [entry, ...state.eventLog].slice(0, MAX_LOG_ENTRIES),
    })),

  setDensity: (density) => set({ density }),

  selectChannel: (selectedChannelId) => set({ selectedChannelId }),
}));
