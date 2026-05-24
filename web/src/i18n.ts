import type { Locale, PluginInfo, SessionState } from "./types";

export const APP_VERSION = "v0.2.0";

export interface AppTranslations {
  appTitle: string;
  versionLabel: string;
  buttons: {
    dismiss: string;
  };
  errors: {
    loadInitialData: string;
    saveTopicMappings: string;
    keyboardShortcut: string;
    transportCommand: string;
    seekCommand: string;
    recordCommand: string;
    playbackCommand: string;
  };
  connectionStatus: Record<"connecting" | "open" | "closed" | "error", string>;
  sessionStates: Record<SessionState, string>;
  pluginStates: Record<PluginInfo["state"], string>;
  menu: {
    file: string;
    view: string;
    plugins: string;
    help: string;
    items: {
      openCapture: { label: string; description: string };
      recentSessions: { label: string; description: string };
      exportSnapshot: { label: string; description: string };
      resetLayout: { label: string; description: string };
      timelineFocus: { label: string; description: string };
      diagnosticsOverlay: { label: string; description: string };
      refreshCatalog: { label: string; description: string };
      scanBundles: { label: string; description: string };
      routeInspector: { label: string; description: string };
      keyboardMap: { label: string; description: string };
      httpApi: { label: string; description: string };
      aboutRecPlay: { label: string; description: string };
    };
    settings: string;
    theme: {
      dark: string;
      light: string;
      switchToDark: string;
      switchToLight: string;
    };
    locale: {
      toggleEnglish: string;
      toggleChinese: string;
      englishShort: string;
      chineseShort: string;
    };
  };
  transport: {
    record: string;
    play: string;
    pause: string;
    stop: string;
    seek: string;
  };
  statusBar: {
    ws: string;
    throughput: string;
    ring: string;
    diskQueue: string;
    cpu: string;
    storageFree: string;
    drops: string;
    console: string;
  };
  timeline: {
    title: string;
    ariaLabel: string;
    clickToSeek: string;
    loop: string;
    dropMarker: (count: number) => string;
  };
  sessionInfo: {
    title: string;
    state: string;
    position: string;
    duration: string;
    speed: string;
    last: string;
    recordingConfig: string;
    playbackConfig: string;
    output: string;
    file: string;
    loopRegion: string;
    start: string;
    end: string;
    files: string;
  };
  pluginTree: {
    title: string;
    empty: string;
    enable: string;
    disable: string;
    error: string;
    up: string;
    down: string;
    select: string;
  };
  pluginCard: {
    empty: string;
    subtitle: string;
    id: string;
    version: string;
    path: string;
    saveConfig: string;
    pluginLog: string;
    entries: string;
    noLogEvents: string;
    actions: {
      active: string;
      inactive: string;
      error: string;
    };
    configLabels: {
      endpoint: string;
      protocol: string;
      buffer: string;
      ttl: string;
      mode: string;
      profile: string;
      health: string;
    };
    configValues: {
      online: string;
      standby: string;
      faulted: string;
      nominal: string;
      defaultProfile: string;
      frames4096: string;
    };
  };
  topicMapper: {
    title: string;
    add: string;
    inputs: string;
    outputs: string;
    noInputs: string;
    noOutputs: string;
    selectInput: string;
    selectOutput: string;
    remove: string;
    noMappings: string;
  };
  monitorDashboard: {
    throughput: string;
    packets: string;
    drops: string;
    p99Latency: string;
    ringBuffer: string;
    diskQueue: string;
    latencyMix: string;
    trendLabels: {
      packets: string;
      drops: string;
      latency: string;
      diskQueue: string;
    };
  };
  recordPanel: {
    title: string;
    outputPath: string;
    protocols: string;
    start: string;
    pause: string;
    resume: string;
    stop: string;
  };
  playbackPanel: {
    title: string;
    filePath: string;
    speed: string;
    seekNs: string;
    loopStart: string;
    loopEnd: string;
    open: string;
    play: string;
    pause: string;
    speedAction: string;
    seekAction: string;
    loopAction: string;
    stop: string;
  };
}

const EN_US: AppTranslations = {
  appTitle: "RecPlay",
  versionLabel: APP_VERSION,
  buttons: { dismiss: "dismiss" },
  errors: {
    loadInitialData: "Failed to load initial data",
    saveTopicMappings: "Failed to save topic mappings",
    keyboardShortcut: "Keyboard shortcut failed",
    transportCommand: "Transport command failed",
    seekCommand: "Seek command failed",
    recordCommand: "Recording command failed",
    playbackCommand: "Playback command failed",
  },
  connectionStatus: {
    connecting: "Connecting",
    open: "Live",
    closed: "Offline",
    error: "Fault",
  },
  sessionStates: {
    Idle: "Idle",
    Recording: "Recording",
    RecordingPaused: "Recording Paused",
    Playing: "Playing",
    PlayingPaused: "Playback Paused",
    Unknown: "Unknown",
  },
  pluginStates: {
    active: "active",
    inactive: "inactive",
    error: "error",
  },
  menu: {
    file: "File",
    view: "View",
    plugins: "Plugins",
    help: "Help",
    items: {
      openCapture: { label: "Open Capture", description: "Load an rpcap or pcap session." },
      recentSessions: { label: "Recent Sessions", description: "Jump back into recent recordings." },
      exportSnapshot: { label: "Export Snapshot", description: "Save the current analysis state." },
      resetLayout: { label: "Reset Layout", description: "Restore the default three-column workspace." },
      timelineFocus: { label: "Timeline Focus", description: "Move focus to the transport timeline." },
      diagnosticsOverlay: { label: "Diagnostics Overlay", description: "Show session counters and runtime hints." },
      refreshCatalog: { label: "Refresh Catalog", description: "Reload bundle metadata from the runtime." },
      scanBundles: { label: "Scan Bundles", description: "Recheck bundle paths and discovered binaries." },
      routeInspector: { label: "Route Inspector", description: "Review topic mappings and plugin wiring." },
      keyboardMap: { label: "Keyboard Map", description: "Review transport and timeline shortcuts." },
      httpApi: { label: "HTTP API", description: "Open the session and playback command reference." },
      aboutRecPlay: { label: "About RecPlay", description: "Show version and runtime capability details." },
    },
    settings: "Open settings",
    theme: {
      dark: "Dark Theme",
      light: "Light Theme",
      switchToDark: "Switch to dark theme",
      switchToLight: "Switch to light theme",
    },
    locale: {
      toggleEnglish: "Switch to English",
      toggleChinese: "Switch to Simplified Chinese",
      englishShort: "EN",
      chineseShort: "中文",
    },
  },
  transport: {
    record: "Record",
    play: "Play",
    pause: "Pause",
    stop: "Stop",
    seek: "Seek",
  },
  statusBar: {
    ws: "WS",
    throughput: "Throughput",
    ring: "Ring",
    diskQueue: "Disk Queue",
    cpu: "CPU",
    storageFree: "Storage Free",
    drops: "Drops",
    console: "RecPlay Console",
  },
  timeline: {
    title: "Timeline",
    ariaLabel: "Transport timeline",
    clickToSeek: "Click to seek",
    loop: "Loop",
    dropMarker: (count) => `+${count} drops`,
  },
  sessionInfo: {
    title: "Session",
    state: "State",
    position: "Position",
    duration: "Duration",
    speed: "Speed",
    last: "Last",
    recordingConfig: "Recording Config",
    playbackConfig: "Playback Config",
    output: "Output",
    file: "File",
    loopRegion: "Loop Region",
    start: "Start",
    end: "End",
    files: "Files",
  },
  pluginTree: {
    title: "Plugins",
    empty: "No plugins loaded",
    enable: "Enable",
    disable: "Disable",
    error: "Error",
    up: "Up",
    down: "Down",
    select: "Select",
  },
  pluginCard: {
    empty: "Select a plugin from the tree",
    subtitle: "Plugin runtime controls",
    id: "ID",
    version: "Version",
    path: "Path",
    saveConfig: "Save Config",
    pluginLog: "Plugin Log",
    entries: "entries",
    noLogEvents: "No plugin log events yet",
    actions: {
      active: "Stop Plugin",
      inactive: "Start Plugin",
      error: "Inspect Fault",
    },
    configLabels: {
      endpoint: "Endpoint",
      protocol: "Protocol",
      buffer: "Buffer",
      ttl: "TTL",
      mode: "Mode",
      profile: "Profile",
      health: "Health",
    },
    configValues: {
      online: "online",
      standby: "standby",
      faulted: "faulted",
      nominal: "nominal",
      defaultProfile: "default",
      frames4096: "4096 frames",
    },
  },
  topicMapper: {
    title: "Topic Mapper",
    add: "+ Add",
    inputs: "Inputs",
    outputs: "Outputs",
    noInputs: "No input channels",
    noOutputs: "No output channels",
    selectInput: "Select input",
    selectOutput: "Select output",
    remove: "Remove",
    noMappings: "No mappings configured",
  },
  monitorDashboard: {
    throughput: "Throughput",
    packets: "Packets",
    drops: "Drops",
    p99Latency: "P99 Latency",
    ringBuffer: "Ring Buffer",
    diskQueue: "Disk Queue",
    latencyMix: "Latency Mix",
    trendLabels: {
      packets: "Packets",
      drops: "Drops",
      latency: "Latency",
      diskQueue: "Disk Queue",
    },
  },
  recordPanel: {
    title: "Record",
    outputPath: "Output Path",
    protocols: "Protocols",
    start: "Start",
    pause: "Pause",
    resume: "Resume",
    stop: "Stop",
  },
  playbackPanel: {
    title: "Playback",
    filePath: "File Path",
    speed: "Speed",
    seekNs: "Seek (ns)",
    loopStart: "Loop Start",
    loopEnd: "Loop End",
    open: "Open",
    play: "Play",
    pause: "Pause",
    speedAction: "Speed",
    seekAction: "Seek",
    loopAction: "Loop",
    stop: "Stop",
  },
};

const ZH_CN: AppTranslations = {
  appTitle: "RecPlay",
  versionLabel: APP_VERSION,
  buttons: { dismiss: "关闭" },
  errors: {
    loadInitialData: "初始数据加载失败",
    saveTopicMappings: "主题映射保存失败",
    keyboardShortcut: "键盘快捷键执行失败",
    transportCommand: "传输控制命令执行失败",
    seekCommand: "定位命令执行失败",
    recordCommand: "录制命令执行失败",
    playbackCommand: "回放命令执行失败",
  },
  connectionStatus: {
    connecting: "连接中",
    open: "在线",
    closed: "离线",
    error: "故障",
  },
  sessionStates: {
    Idle: "空闲",
    Recording: "录制中",
    RecordingPaused: "录制已暂停",
    Playing: "回放中",
    PlayingPaused: "回放已暂停",
    Unknown: "未知",
  },
  pluginStates: {
    active: "运行中",
    inactive: "未启用",
    error: "错误",
  },
  menu: {
    file: "文件",
    view: "视图",
    plugins: "插件",
    help: "帮助",
    items: {
      openCapture: { label: "打开抓包", description: "加载 rpcap 或 pcap 会话。" },
      recentSessions: { label: "最近会话", description: "快速回到最近的录制数据。" },
      exportSnapshot: { label: "导出快照", description: "保存当前分析状态。" },
      resetLayout: { label: "重置布局", description: "恢复默认三栏工作区。" },
      timelineFocus: { label: "聚焦时间线", description: "将焦点移动到传输时间线。" },
      diagnosticsOverlay: { label: "诊断叠层", description: "显示会话计数器和运行时提示。" },
      refreshCatalog: { label: "刷新目录", description: "重新加载运行时中的 bundle 元数据。" },
      scanBundles: { label: "扫描 Bundles", description: "重新检查 bundle 路径和已发现的二进制文件。" },
      routeInspector: { label: "路由检查器", description: "查看主题映射和插件连线。" },
      keyboardMap: { label: "快捷键说明", description: "查看传输控制和时间线快捷键。" },
      httpApi: { label: "HTTP API", description: "打开会话和回放命令参考。" },
      aboutRecPlay: { label: "关于 RecPlay", description: "显示版本和运行时能力信息。" },
    },
    settings: "打开设置",
    theme: {
      dark: "深色主题",
      light: "浅色主题",
      switchToDark: "切换到深色主题",
      switchToLight: "切换到浅色主题",
    },
    locale: {
      toggleEnglish: "切换到英文",
      toggleChinese: "切换到简体中文",
      englishShort: "EN",
      chineseShort: "中文",
    },
  },
  transport: {
    record: "录制",
    play: "播放",
    pause: "暂停",
    stop: "停止",
    seek: "定位",
  },
  statusBar: {
    ws: "连接",
    throughput: "吞吐",
    ring: "环形缓冲",
    diskQueue: "磁盘队列",
    cpu: "CPU",
    storageFree: "剩余存储",
    drops: "丢包",
    console: "RecPlay 控制台",
  },
  timeline: {
    title: "时间线",
    ariaLabel: "传输时间线",
    clickToSeek: "点击定位",
    loop: "循环区间",
    dropMarker: (count) => `+${count} 次丢包`,
  },
  sessionInfo: {
    title: "会话",
    state: "状态",
    position: "位置",
    duration: "时长",
    speed: "速度",
    last: "最近一次",
    recordingConfig: "录制配置",
    playbackConfig: "回放配置",
    output: "输出",
    file: "文件",
    loopRegion: "循环区间",
    start: "开始",
    end: "结束",
    files: "文件列表",
  },
  pluginTree: {
    title: "插件",
    empty: "未加载任何插件",
    enable: "启用",
    disable: "停用",
    error: "错误",
    up: "上移",
    down: "下移",
    select: "选择",
  },
  pluginCard: {
    empty: "请先从树中选择一个插件",
    subtitle: "插件运行时控制",
    id: "标识",
    version: "版本",
    path: "路径",
    saveConfig: "保存配置",
    pluginLog: "插件日志",
    entries: "条",
    noLogEvents: "暂无插件日志事件",
    actions: {
      active: "停止插件",
      inactive: "启动插件",
      error: "检查故障",
    },
    configLabels: {
      endpoint: "端点",
      protocol: "协议",
      buffer: "缓冲区",
      ttl: "TTL",
      mode: "模式",
      profile: "配置档",
      health: "健康状态",
    },
    configValues: {
      online: "在线",
      standby: "待机",
      faulted: "故障",
      nominal: "正常",
      defaultProfile: "默认",
      frames4096: "4096 帧",
    },
  },
  topicMapper: {
    title: "主题映射",
    add: "+ 新增",
    inputs: "输入",
    outputs: "输出",
    noInputs: "没有输入通道",
    noOutputs: "没有输出通道",
    selectInput: "选择输入",
    selectOutput: "选择输出",
    remove: "移除",
    noMappings: "尚未配置映射",
  },
  monitorDashboard: {
    throughput: "吞吐",
    packets: "报文数",
    drops: "丢包",
    p99Latency: "P99 延迟",
    ringBuffer: "环形缓冲",
    diskQueue: "磁盘队列",
    latencyMix: "延迟分布",
    trendLabels: {
      packets: "报文数",
      drops: "丢包",
      latency: "延迟",
      diskQueue: "磁盘队列",
    },
  },
  recordPanel: {
    title: "录制",
    outputPath: "输出路径",
    protocols: "协议",
    start: "开始",
    pause: "暂停",
    resume: "继续",
    stop: "停止",
  },
  playbackPanel: {
    title: "回放",
    filePath: "文件路径",
    speed: "速度",
    seekNs: "定位 (ns)",
    loopStart: "循环开始",
    loopEnd: "循环结束",
    open: "打开",
    play: "播放",
    pause: "暂停",
    speedAction: "设速",
    seekAction: "定位",
    loopAction: "循环",
    stop: "停止",
  },
};

const TRANSLATIONS: Record<Locale, AppTranslations> = {
  "en-US": EN_US,
  "zh-CN": ZH_CN,
};

export function getTranslations(locale: Locale): AppTranslations {
  return TRANSLATIONS[locale];
}

export function formatSessionStateLabel(state: SessionState, locale: Locale): string {
  return getTranslations(locale).sessionStates[state];
}

export function formatPluginStateLabel(state: PluginInfo["state"], locale: Locale): string {
  return getTranslations(locale).pluginStates[state];
}
