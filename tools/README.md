# RecPlay E2E Tools

这套工装用于验证 `UDP 采集 -> 录制 -> 回放 -> 可视化` 的完整闭环。

## 文件

- `sim_aircraft.py`: 按固定频率向 `239.1.1.1:5000` 发送模拟航迹 UDP 包。
- `udp_ws_bridge.py`: 加入同一组播组，把收到的 UDP JSON 原样转成 WebSocket 文本消息。
- `trajectory_viewer.html`: 浏览器里的实时航迹显示页。
- `e2e_record_replay.ps1`: 录制、停止、回放的流程脚本。

## 运行顺序

1. 启动 RecPlay。
2. 启动桥：

```powershell
python tools/udp_ws_bridge.py
```

3. 打开显示器：

```powershell
start tools/trajectory_viewer.html
```

4. 开始流程：

```powershell
powershell -ExecutionPolicy Bypass -File tools/e2e_record_replay.ps1
```

5. 脚本提示后再启动模拟器：

```powershell
python tools/sim_aircraft.py
```

## 默认约定

- UDP multicast: `239.1.1.1:5000`
- Bridge WS: `ws://localhost:8765`
- 输出文件: `data/flight1.rpcap`

## 依赖

- Python 3.10+
- 浏览器
- RecPlay 正在运行

## 排错

- 如果桥收不到包，检查 Windows 防火墙和组播回环。
- 如果回放没画出轨迹，确认 `open` 请求里带了 `protocol_config`。
- 如果浏览器连不上桥，确认 `udp_ws_bridge.py` 已经在监听 `8765`。
