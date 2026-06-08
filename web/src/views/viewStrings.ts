import type { Locale, LogLevel } from "../types";

interface ViewStrings {
  common: {
    session: string;
    unavailable: string;
    mock: string;
    settings: string;
    search: string;
    record: string;
    replay: string;
    dark: string;
    light: string;
    language: string;
  };
  recorder: {
    sources: string;
    sinks: string;
    liveThroughput: string;
    channels: string;
    inspector: string;
    eventLog: string;
    start: string;
    stop: string;
    pause: string;
    resume: string;
    autoSegment: string;
    codec: string;
    captureBuffer: string;
    queue: string;
  };
  player: {
    captures: string;
    channelMap: string;
    markers: string;
    eventDensity: string;
    fileMetadata: string;
    packetInspector: string;
    hexDump: string;
    replayTargets: string;
    play: string;
    pause: string;
    previousPacket: string;
    nextPacket: string;
    reset: string;
    setLoop: string;
    inject: string;
    bookmark: string;
    targetEnableAll: string;
    targetAdd: string;
  };
  timeline: {
    position: string;
    rate: string;
    bytes: string;
    drift: string;
    clickHint: string;
    shiftLoopHint: string;
  };
  settings: {
    theme: string;
    density: string;
    accent: string;
    timelineStyle: string;
    rateControl: string;
    pluginLayout: string;
    compact: string;
    regular: string;
    comfortable: string;
    buttons: string;
    slider: string;
    knob: string;
    list: string;
    grid: string;
    close: string;
  };
  logLevels: Record<LogLevel, string>;
}

const EN_US: ViewStrings = {
  common: {
    session: "Session",
    unavailable: "Unavailable",
    mock: "mock",
    settings: "Settings",
    search: "Search",
    record: "Record",
    replay: "Replay",
    dark: "Dark",
    light: "Light",
    language: "Language",
  },
  recorder: {
    sources: "Sources",
    sinks: "Sinks",
    liveThroughput: "Live Throughput",
    channels: "Channels",
    inspector: "Inspector",
    eventLog: "Event Log",
    start: "Start Record",
    stop: "Stop Record",
    pause: "Pause",
    resume: "Resume",
    autoSegment: "auto-segment",
    codec: "codec",
    captureBuffer: "capture buffer",
    queue: "queue",
  },
  player: {
    captures: "Captures",
    channelMap: "Channel Map",
    markers: "Markers",
    eventDensity: "Event Density",
    fileMetadata: "File Metadata",
    packetInspector: "Packet Inspector",
    hexDump: "Hex Dump",
    replayTargets: "Replay Targets",
    play: "Play",
    pause: "Pause",
    previousPacket: "Prev Packet",
    nextPacket: "Next Packet",
    reset: "Back to Start",
    setLoop: "Set Loop",
    inject: "Inject",
    bookmark: "Bookmark",
    targetEnableAll: "Enable All",
    targetAdd: "Add",
  },
  timeline: {
    position: "Position",
    rate: "Replay Rate",
    bytes: "Bytes Replayed",
    drift: "Clock Drift",
    clickHint: "Click to seek",
    shiftLoopHint: "Shift+click to set loop",
  },
  settings: {
    theme: "Theme",
    density: "Density",
    accent: "Accent",
    timelineStyle: "Timeline Style",
    rateControl: "Rate Control",
    pluginLayout: "Plugin Layout",
    compact: "Compact",
    regular: "Regular",
    comfortable: "Comfortable",
    buttons: "Buttons",
    slider: "Slider",
    knob: "Knob",
    list: "List",
    grid: "Grid",
    close: "Close",
  },
  logLevels: {
    ok: "OK",
    info: "INFO",
    warn: "WARN",
    err: "ERR",
  },
};

const ZH_CN: ViewStrings = {
  common: {
    session: "会话",
    unavailable: "不可用",
    mock: "模拟",
    settings: "设置",
    search: "搜索",
    record: "录制",
    replay: "回放",
    dark: "暗色",
    light: "亮色",
    language: "语言",
  },
  recorder: {
    sources: "数据源",
    sinks: "输出端",
    liveThroughput: "实时吞吐",
    channels: "通道",
    inspector: "检视",
    eventLog: "事件日志",
    start: "开始录制",
    stop: "停止录制",
    pause: "暂停",
    resume: "继续",
    autoSegment: "自动分段",
    codec: "编解码",
    captureBuffer: "采集缓冲",
    queue: "队列",
  },
  player: {
    captures: "录制文件",
    channelMap: "通道映射",
    markers: "标记",
    eventDensity: "事件密度",
    fileMetadata: "文件元数据",
    packetInspector: "数据包检视",
    hexDump: "Hex Dump",
    replayTargets: "回放目标",
    play: "播放",
    pause: "暂停",
    previousPacket: "上一包",
    nextPacket: "下一包",
    reset: "回到起点",
    setLoop: "设循环",
    inject: "注入",
    bookmark: "书签",
    targetEnableAll: "启用全部",
    targetAdd: "添加",
  },
  timeline: {
    position: "位置",
    rate: "回放速率",
    bytes: "回放字节",
    drift: "时钟漂移",
    clickHint: "点击定位",
    shiftLoopHint: "Shift+点击设置循环",
  },
  settings: {
    theme: "主题",
    density: "信息密度",
    accent: "强调色",
    timelineStyle: "时间轴样式",
    rateControl: "倍速控件",
    pluginLayout: "插件布局",
    compact: "紧凑",
    regular: "标准",
    comfortable: "舒适",
    buttons: "按钮",
    slider: "滑杆",
    knob: "旋钮",
    list: "列表",
    grid: "网格",
    close: "关闭",
  },
  logLevels: {
    ok: "正常",
    info: "信息",
    warn: "警告",
    err: "错误",
  },
};

const STRINGS: Record<Locale, ViewStrings> = {
  "en-US": EN_US,
  "zh-CN": ZH_CN,
};

export function getViewStrings(locale: Locale): ViewStrings {
  return STRINGS[locale];
}
