# RecPlay E2E Tools

这套工装验证 `UDP 采集 → 录制 → 回放 → 可视化 → 接收验证` 的完整闭环。

---

## 文件一览

| 文件 | 说明 |
|------|------|
| `flight_sim.py` | **完整飞行剖面模拟器**（8 阶段：TAXI_OUT → TAKEOFF → CLIMB → CRUISE → DESCENT → APPROACH → LANDING → TAXI_IN）|
| `recv.py` | **回放数据接收验证器**：统计序列间隙、时序抖动、各阶段覆盖率，支持 CSV 导出 |
| `udp_ws_bridge.py` | UDP 组播 → WebSocket 桥，供浏览器实时可视化 |
| `trajectory_viewer.html` | 浏览器航迹显示页（含高度历史图、飞行阶段标识） |
| `e2e_record_replay.ps1` | 录制 → 停止 → 回放自动化流程脚本 |

---

## 快速开始（5 步）

```
1. 启动 RecPlay
2. python tools/udp_ws_bridge.py
3. 打开 tools/trajectory_viewer.html（浏览器）
4. powershell -ExecutionPolicy Bypass -File tools/e2e_record_replay.ps1
5. python tools/flight_sim.py               ← 提示录制开始后运行
```

回放结束后，用接收器验证回放质量：

```
python tools/recv.py
```

---

## flight_sim.py — 飞行模拟器

### 功能
- 完整 8 阶段飞行剖面，每阶段有物理意义的速度/高度/姿态变化
- 5 条预置航路（可扩展）
- 多航班同时仿真（各自独立 UDP 流）
- 兼容旧 `sim_aircraft.py` 的 JSON 字段，增加了 `vspd_fpm`、`bank_deg`、`pitch_deg`、`phase`、`gear`、`flaps`

### 数据包格式（JSON over UDP）
```json
{
  "id":        "CCA1234",
  "seq":       42,
  "t_ms":      8400,
  "lat":       36.5,
  "lon":       118.2,
  "alt_m":     8500.0,
  "hdg_deg":   125.3,
  "spd_kt":    447.2,
  "vspd_fpm":  -120,
  "bank_deg":  14.5,
  "pitch_deg": 2.5,
  "phase":     "CRUISE",
  "gear":      0,
  "flaps":     0
}
```

### 常用命令
```bash
# 默认：北京→上海，5 分钟，5 Hz
python tools/flight_sim.py

# 选择航路
python tools/flight_sim.py --route PVG-CAN
python tools/flight_sim.py --route HND-ZBAA

# 列出所有航路
python tools/flight_sim.py --list-routes

# 10 分钟完整飞行
python tools/flight_sim.py --duration-sec 600

# 3 架飞机同时，错峰 30 秒起飞
python tools/flight_sim.py --count 3 --stagger-sec 30

# 自定义呼号 + 详细输出
python tools/flight_sim.py --id CES888 -v

# 高频率（测试 RecPlay 存储压力）
python tools/flight_sim.py --rate-hz 50 --duration-sec 120
```

### 可用航路

| 路由码 | 航线 | 距离 |
|--------|------|------|
| `PEK-PVG` | 北京首都 → 上海浦东 | 1,070 km |
| `PVG-CAN` | 上海浦东 → 广州白云 | 1,140 km |
| `PEK-CAN` | 北京首都 → 广州白云 | 1,900 km |
| `HND-ZBAA` | 东京羽田 → 北京首都 | 2,100 km |
| `ICN-ZSPD` | 首尔仁川 → 上海浦东 | 870 km |

---

## recv.py — 回放接收验证器

### 功能
- 加入与模拟器相同的组播组，接收 RecPlay 回放的数据包
- 统计每架飞机：序列连续性（间隙计数）、平均/p95 时序抖动、高度/速度范围、飞行阶段覆盖情况
- 可选 CSV 导出，供离线分析（Excel、Python pandas、Matlab 等）

### 常用命令
```bash
# 监听默认组播，Ctrl+C 后打印摘要
python tools/recv.py

# 自动 120 秒后退出
python tools/recv.py --duration-sec 120

# 同时写 CSV
python tools/recv.py --csv data/replay_check.csv

# 打印每一条数据包
python tools/recv.py --verbose

# 回放发往单播地址时
python tools/recv.py --address 127.0.0.1
```

### 输出示例
```
Joined multicast 239.1.1.1:5000
Waiting for packets… (Ctrl+C to stop)

[NEW] Aircraft: CCA1234
[CCA1234]  # 50  CLIMB      lat=39.2000  lon=117.3000  alt=  4200m  spd=280.0kt
[CCA1234]  #100  CRUISE     lat=37.8000  lon=118.5000  alt= 10080m  spd=451.2kt
...

========================================================================
SUMMARY  —  1 aircraft  |  1500 total packets  |  0 decode errors

  [CCA1234]
    Packets   : 1500  seq_gaps: 0
    Rate      : 5.00 Hz   duration: 300.0 s
    Altitude  : 35 – 10120 m
    Speed     : 0 – 462 kt
    Phases    : TAXI_OUT → TAKEOFF → CLIMB → CRUISE → DESCENT → APPROACH → LANDING → TAXI_IN
    Jitter    : avg 0.8 ms   p95 2.1 ms
========================================================================
```

---

## 默认约定

| 项目 | 默认值 |
|------|--------|
| UDP 组播地址 | `239.1.1.1` |
| UDP 端口 | `5000` |
| WebSocket 桥端口 | `8765` |
| 输出录制文件 | `data/flight1.rpcap` |
| RecPlay HTTP API | `http://localhost:8080` |

---

## 依赖

- Python 3.10+（标准库，无需额外安装）
- RecPlay 正在运行
- 浏览器（用于 trajectory_viewer.html）

---

## 排错

| 问题 | 检查项 |
|------|--------|
| 桥收不到包 | Windows 防火墙、组播回环（`IP_MULTICAST_LOOP=1`） |
| 回放没画出轨迹 | 确认 `/api/session/playback/open` 请求带了 `protocol_config` |
| `recv.py` 序列间隙多 | 可能是 RecPlay 回放时丢帧（降低 `--rate-hz` 或检查存储 IO） |
| 浏览器连不上桥 | 确认 `udp_ws_bridge.py` 监听在 8765 |
