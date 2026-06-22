# M5AtomS3 → Windows 语音输入方案

## 目标

把 **M5AtomS3**（ESP32-S3 + 内置 PDM 麦克风）变成一个蓝牙一键即讲（Push-to-Talk）语音输入设备，连接 Windows，语音转文字后自动粘贴到当前输入框。

对标 [78/voicestick](https://github.com/78/voicestick)（StickS3 + macOS），但适配 AtomS3 硬件 + Windows 平台。

---

## 硬件

**M5AtomS3** 自带资源：
- ESP32-S3（双核 240MHz, 8MB PSRAM）
- PDM 麦克风 **MSM261DCM**（内置，无需外接模块）
- 5×5 RGB LED 矩阵（SK6812，GPIO35）
- 单按键（GPIO39）
- USB-C + BLE 5.0

参考文档：[M5AtomS3 官方规格](https://docs.m5stack.com/en/core/AtomS3)

---

## 整体架构

```
AtomS3 PDM mic → Opus 编码 → BLE → Windows 桌面端 → ASR → 粘贴到当前窗口
```

### 固件（ESP-IDF / Arduino）

| 模块 | 说明 | 参考 |
|------|------|------|
| **PDM 麦克风驱动** | AtomS3 内置 MSM261DCM，PDM 转 PCM 16kHz/16bit | [birdnetgo-atoms3-pdm](https://github.com/matthew73210/birdnetgo-m5stack-AtomS3-Lite-PDM-rtsp-mic) — PDM 采集完整实现 |
| **Opus 编码** | 把 PCM 压缩成 Opus 帧，降低 BLE 带宽 | [voicestick firmware](https://github.com/78/voicestick/tree/main/firmware) — 已有 Opus 编码实现 |
| **BLE 传输** | GATT notify，把 Opus 帧发给 Windows | [voicestick BLE 协议](https://github.com/78/voicestick/blob/main/docs/protocol.md) — GATT service UUID + 音频/状态/控制通道 |
| **RGB LED 显示** | 替代 LCD 显示状态：待机蓝、录音红、识别中黄、确认绿 | 参考 BLE-sensor-pdm |
| **按键逻辑** | 单按钮：按住录音，松手发送；双击取消 | 参考 voicestick 按键状态机 |

PDM 参考项目：[TIT8/BLE-sensor_PDM-microphone](https://github.com/TIT8/BLE-sensor_PDM-microphone) — PDM → BLE 架构

### Windows 桌面端（C++/WinRT 或 Python）

| 模块 | 说明 | 参考 |
|------|------|------|
| **BLE 客户端** | 扫描 VS-XXXX 设备，连接，接收 notify | [voicestick windows](https://github.com/78/voicestick/tree/main/desktop/windows) — Win32/C++20，C++/WinRT BLE，已有完整骨架 |
| **Ogg Opus 复用** | 将 BLE 收到的 Opus 帧封装成 Ogg Opus 文件 | 同上，已有实现 |
| **ASR 识别** | WebSocket 转发到火山引擎或其他 ASR | 同上，支持 Volcengine 或 VoiceStick Cloud relay |
| **文本粘贴** | 识别结果通过 SendInput / clipboard 模拟粘贴 + 回车 | 同上，已有 `SendInput` 实现 |
| **托盘 UI** | 系统托盘 + 悬浮窗显示识别状态 | 同上，已有 |

备选方案：[openbrt/voxstick](https://github.com/openbrt/voxstick) — 类似项目，USB 方式（非 BLE），但也走 Windows 粘贴链路

---

## Home Assistant 备选路线

如果最终目标是语音控制而非文字输入，可以用 ESPHome 直接刷：

**AtomS3 + Atomic Voice Base**（需额外买底座）→ **Home Assistant Voice Assistant**：
- [M5Stack 官方教程](https://docs.m5stack.com/en/homeassistant/voice_assistant/atoms3r_with_atomic_echo_base_voice_assistant)
- 走 WiFi + ESPHome，支持唤醒词，直接控制 HA 设备
- 但**不能**实现 Windows 任意窗口粘贴文字

---

## 实现路径

### 方案 A — 改 VoiceStick（推荐，改动最小）

复用 voicestick 的 **Windows 端不变**，只改固件：

1. **固件**：把 voicestick 的 `firmware/` 中音频采集从 ES8311 I2S 改成 AtomS3 PDM
   - 改 `components/stick_s3_board/` → AtomS3 pin 定义
   - 改音频驱动 → 用 ESP-IDF `i2s_read()` + `I2S_PDM` 模式
   - 去掉 LCD 驱动，换成 RGB LED 矩阵控制
   - 按键从 2 个改成 1 个（长按/短按/双击区分）
   - BLE 协议层不改，保持与 Windows 端兼容
2. **Windows 端**：直接编译 voicestick 的 `desktop/windows/`，理论上不动

### 方案 B — 从零写轻量固件

如果觉得 voicestick 固件太复杂（ES8311 + LCD + 双按键 + 电池管理都要砍），可以基于 PDM 参考项目新建一个精简固件：
- 只做 PDM 采集 + Opus + BLE notify
- 参考 [birdnetgo-atoms3-pdm](https://github.com/matthew73210/birdnetgo-m5stack-AtomS3-Lite-PDM-rtsp-mic) 的 PDM 部分
- 参考 [TIT8/BLE-sensor_PDM-microphone](https://github.com/TIT8/BLE-sensor_PDM-microphone) 的 BLE 架构
- 然后适配 voicestick 的 Windows 端

---

## 参考链接汇总

| 项目 | 链接 | 用途 |
|------|------|------|
| VoiceStick（总项目） | https://github.com/78/voicestick | 核心参考：Windows 端 + BLE 协议 + Opus 编码 |
| VoiceStick 固件 | https://github.com/78/voicestick/tree/main/firmware | BLE/Opus 实现，需修改音频驱动 |
| VoiceStick Windows 端 | https://github.com/78/voicestick/tree/main/desktop/windows | Windows 端直接可用 |
| VoiceStick BLE 协议 | https://github.com/78/voicestick/blob/main/docs/protocol.md | GATT UUID、帧格式 |
| BirdNET-Go AtomS3 PDM | https://github.com/matthew73210/birdnetgo-m5stack-AtomS3-Lite-PDM-rtsp-mic | PDM 麦克风采集实现参考 |
| BLE PDM 传感器 | https://github.com/TIT8/BLE-sensor_PDM-microphone | PDM → BLE 架构参考 |
| VoxStick（USB 方案） | https://github.com/openbrt/voxstick | USB 替代方案参考 |
| M5AtomS3 官方文档 | https://docs.m5stack.com/en/core/AtomS3 | 硬件 pin 定义 |
| AtomS3R + HA 语音 | https://docs.m5stack.com/en/homeassistant/voice_assistant/atoms3r_with_atomic_echo_base_voice_assistant | HA 备选路线 |
