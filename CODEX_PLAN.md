# CODEX_PLAN — 前端框架重构进度

## 总体状态: 骨架已完成，等待 Codex 扩展填充

---

## 已完成 ✅

### 1. 依赖安装
- [x] `zustand` ^5.0.0
- [x] `tailwindcss` ^4.1.0 + `@tailwindcss/vite` ^4.1.0
- [x] `echarts` ^5.6.0 + `echarts-for-react` ^3.0.2

### 2. TailwindCSS 配置
- [x] `web/src/styles.css` — `@import "tailwindcss"` + HMI 暗色工业风主题 (`@theme`)
- [x] `web/vite.config.ts` — 添加 `tailwindcss()` 插件
- [x] `web/tsconfig.app.json` — `moduleResolution: "Bundler"`

### 3. 类型定义
- [x] `web/src/types/index.ts` — SessionState, StatsSnapshot, PluginInfo, TimelineState, TimelineMarker, Channel, TopicMapping

### 4. Zustand Stores
- [x] `web/src/stores/sessionStore.ts` — replaceSnapshot / applyTransition
- [x] `web/src/stores/statsStore.ts` — replaceSnapshot (含 history 滚动窗口)
- [x] `web/src/stores/pluginStore.ts` — setPlugins / updatePlugin / selectPlugin
- [x] 删除旧 `web/src/stores/createStore.ts`

### 5. WebSocket Hook
- [x] `web/src/hooks/useWebSocket.ts` — 指数退避重连 + `timeline` / `plugin_event` 消息处理

### 6. API Client 增强
- [x] `web/src/api/client.ts` — 新增 getPlugins, getPluginDetail, getTopicMappings, setTopicMappings, getChannels, listRecordingFiles

### 7. 布局 (三栏)
- [x] `web/src/App.tsx` — MenuBar / [PluginTree | Center | SessionInfo] / TransportBar / StatusBar

### 8. 骨架组件
- [x] `web/src/components/MenuBar/MenuBar.tsx`
- [x] `web/src/components/TransportBar/TransportBar.tsx`
- [x] `web/src/components/StatusBar/StatusBar.tsx`
- [x] `web/src/components/SessionInfo/SessionInfo.tsx`
- [x] `web/src/components/Timeline/Timeline.tsx` — ECharts sparkline + playhead
- [x] `web/src/components/PluginTree/PluginTree.tsx` — 树形列表
- [x] `web/src/components/PluginCard/PluginCard.tsx` — 插件配置卡片
- [x] `web/src/components/TopicMapper/TopicMapper.tsx` — Patch Bay
- [x] `web/src/components/MonitorDashboard/MonitorDashboard.tsx` — ECharts 图表 + 指标卡

### 9. 现有组件适配
- [x] `web/src/components/RecordPanel/RecordPanel.tsx` — Zustand + Tailwind
- [x] `web/src/components/PlaybackPanel/PlaybackPanel.tsx` — Zustand + Tailwind

### 10. 验证
- [x] `npm install` 成功
- [x] `tsc -b` 无报错
- [x] `vite build` 成功 (dist 产出)
- [x] `vitest` 6/6 测试通过

---

## Codex 待扩展 (TODO)

以下是各组件中 `// TODO: Codex` 标注的扩展点：

### MenuBar
- 实现下拉菜单 (File/View/Plugins/Help)
- 连接状态指示器 + 设置图标

### TransportBar
- 将按钮 click 绑定到 API 调用 (startRecording, playPlayback, etc.)
- 进度条支持点击/拖拽 seek

### StatusBar
- 添加系统时钟、CPU 使用率、磁盘空间指示

### Timeline
- 交互式 playhead 拖拽
- 缩放控制
- TimelineMarker 显示 (书签/事件/错误)
- 点击定位 (click-to-seek)
- Loop 区域叠加层

### PluginTree
- 拖拽排序
- 右键菜单
- 启用/禁用开关

### PluginCard
- 插件专属配置表单 (动态字段)
- 启动/停止控件
- 插件日志查看器

### TopicMapper
- 拖拽连线式 Patch Bay UI (SVG/Canvas)
- 连接线可视化

### MonitorDashboard
- Ring Buffer 仪表盘 (gauge)
- 延迟分布柱状图
- 更多时序图表类型

### 其他
- 深色/浅色主题切换
- 国际化支持
- 键盘快捷键
- 响应式布局优化 (移动端)

---

## 构建指令

```bash
cd web
npm install
npm run dev      # 开发服务器 http://127.0.0.1:5173
npm run build    # 生产构建
npm test         # 运行测试
```

## 三栏布局结构

```
┌─────────────────────────────────────────────────────────────┐
│ MenuBar                                                      │
├────────────┬──────────────────────────────�┬─────────────────┤
│ PluginTree │ Timeline                     │ SessionInfo     │
│            │ MonitorDashboard             │                 │
│            │ RecordPanel / PlaybackPanel  │                 │
│            │ PluginCard (selected)        │                 │
├────────────┴──────────────────────────────┴─────────────────┤
│ TransportBar                                                 │
├─────────────────────────────────────────────────────────────┤
│ StatusBar                                                    │
└─────────────────────────────────────────────────────────────┘
```
