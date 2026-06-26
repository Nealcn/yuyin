# M5AtomS3R Voice Stick — 蓝牙语音输入棒

将 **M5AtomS3R AI Chatbot + Atomic Echo Base** 变成一个蓝牙一键即讲（Push-to-Talk）语音输入设备，连接 Windows，语音转文字后自动粘贴到当前输入框。

对标 [78/voicestick](https://github.com/78/voicestick)（StickS3 + macOS），但适配 AtomS3R + Echo Base 硬件及 Windows 平台。

---

## 硬件

### M5AtomS3R AI Chatbot（主机身）

| 组件 | 规格 |
|------|------|
| **主控** | ESP32-S3-PICO-1-N8R8（双核 240MHz，revision v0.2） |
| **Flash** | 8MB（分区表限制 4MB） |
| **PSRAM** | 8MB OPI |
| **屏幕** | 0.85寸 128×128 彩色 IPS LCD（GC9107，SPI） |
| **背光** | LP5562 驱动（I²C: SDA=GPIO45, SCL=GPIO0） |
| **PDM 麦克风** | 板载 MSM381A3729H9BPC（**已弃用**，因 ESP32-S3 v0.2 I2S PDM 外设无法可靠工作） |
| **按键** | GPIO41（实测 GPIO41 可用） |
| **USB** | USB-C（供电 + 串口） |

### Atomic Echo Base（音频底座）

通过底部排针与 AtomS3R 连接，提供 ES8311 音频编解码器：

| 组件 | 规格 |
|------|------|
| **编解码器** | ES8311（低功耗 I2S 音频 codec） |
| **麦克风** | 板载驻极体麦克风（模拟输入） |
| **喇叭** | 板载 0.5W 喇叭（DAC 输出） |
| **I/O 扩展** | PCA9557（I²C 转 GPIO，用于 ES8311 电源控制） |
| **I²C 地址** | ES8311=0x18, PCA9557=0x43 |

### ES8311 引脚定义

| 信号 | GPIO | 说明 |
|------|------|------|
| **I2C_SDA** | GPIO38 | I²C 数据线 |
| **I2C_SCL** | GPIO39 | I²C 时钟线 |
| **I2S_BCK** | GPIO8 | I²S 位时钟 |
| **I2S_WS** | GPIO6 | I²S 字选择（LRCLK） |
| **I2S_DIN** | GPIO7 | I²S 音频数据输入（麦克风→ESP32） |
| **I2S_DOUT** | GPIO5 | I²S 音频数据输出（ESP32→喇叭） |

### LCD 引脚定义

| 信号 | GPIO | 说明 |
|------|------|------|
| **SCK** | GPIO15 | SPI 时钟（20MHz） |
| **MOSI** | GPIO21 | SPI 数据 |
| **CS** | GPIO14 | 片选 |
| **DC** | GPIO42 | 数据/命令选择 |
| **RST** | GPIO48 | 复位 |
| **BL** | LP5562 (I²C: GPIO45/0) | 背光（通过 LP5562 驱动） |

> **注意：** AtomS3R 和 AtomS3 不同。AtomS3 只有 5×5 RGB LED 矩阵，**AtomS3R 有 0.85寸 LCD 屏幕**。本固件仅支持 AtomS3R + Atomic Echo Base 组合。

---

## 项目结构

```
d:\WAN\yuyinzhushou\
├── firmware/                    # ESP-IDF 固件
│   ├── CMakeLists.txt
│   ├── sdkconfig                # 编译配置
│   ├── partitions.csv           # 分区表
│   ├── main/                    # 主程序
│   │   ├── main.c              # 入口 + 状态机
│   │   ├── board_config.h      # 引脚定义（含 ES8311）
│   │   ├── pdm_mic.c/h         # 音频采集（封装 ES8311）
│   │   ├── es8311_codec.c/h    # ES8311 编解码器驱动（I²C + I2S）
│   │   ├── opus_encoder.c/h    # Opus 音频编码
│   │   ├── ble_service.c/h     # BLE GATT 服务（NimBLE）
│   │   ├── display.c/h         # GC9107 LCD 驱动 + LP5562 背光
│   │   ├── button.c/h          # 按键驱动（GPIO41 中断）
│   │   └── CMakeLists.txt
│   └── components/
│       └── opus/               # Opus 编解码器源码
│           └── opus_src/       # opus-1.4 源码
├── desktop/
│   ├── windows/                 # Windows 桌面端（C++，未维护）
│   │   ├── CMakeLists.txt
│   │   └── src/
│   └── python/                  # Python 桌面端（推荐，活跃开发）
│       ├── main.py             # 入口
│       ├── requirements.txt
│       └── voicestick/
│           ├── app.py          # 系统托盘 + 主逻辑
│           ├── ble.py          # BLE 客户端 (bleak)
│           ├── protocol.py     # 协议解析
│           ├── asr_client.py   # 火山引擎 ASR WebSocket
│           ├── asr_protocol.py # ASR 二进制协议
│           ├── ogg_opus_muxer.py # Ogg Opus 封装
│           ├── coordinator.py  # 状态机
│           ├── input_injector.py # 文本注入（Win32 SendInput）
│           ├── config.py       # 配置管理
│           └── ui/             # PyQt5 UI
├── docs/
│   ├── protocol.md             # BLE 协议文档
│   └── pdm-debug-log.md        # PDM 麦克风调试记录
├── flash_fw.py                 # 编译+烧录脚本（自动编译→COM3）
├── build_desktop.bat           # Windows 桌面端编译脚本（C++）
├── build_desktop.ps1           # Windows 桌面端编译脚本（备用）
├── dl_vs.ps1                   # 下载 VS Build Tools
└── README.md                   # 本文件
```

---

## 开发环境

### 工具链

| 工具 | 位置 |
|------|------|
| **ESP-IDF** | `D:\esp-idf`（v5.5.4） |
| **编译工具链** | `D:\espressif\tools\` |
| **Python 环境** | `C:\Users\Administrator\.espressif\python_env\idf5.5_py3.10_env\` |
| **CMake** | `D:\espressif\tools\cmake\3.30.2\bin\cmake.exe` |
| **Ninja** | `D:\espressif\tools\ninja\1.12.1\ninja.exe` |
| **Xtensa 工具链** | `D:\espressif\tools\xtensa-esp-elf\esp-14.2.0_20260121\` |

### 编译固件

```bash
cd /d/WAN/yuyinzhushou
python flash_fw.py
```

会自动编译并烧录到 COM3。

### 运行 Python 桌面端

```bash
cd /d/WAN/yuyinzhushou/desktop/python
d:\program\miniconda\python.exe main.py
```

首次运行需先安装依赖：
```bash
pip install bleak aiohttp PyQt5
```

在系统托盘中操作：右键图标 → **设备 → 扫描配对** → 选择 VS-E3F6。

### 查看串口输出

```bash
cd /d/WAN/yuyinzhushou
python monitor.py
```

---

## 架构

```
┌──────────────────────────────┐     BLE GATT     ┌──────────────────────────────┐
│  M5AtomS3R + Atomic Echo Base│  ◄──────────►   │  Windows 桌面端（Python）    │
│                              │  notify/控制     │                              │
│  ES8311 I2S → 32kHz PCM     │                 │  BLE 接收 → Ogg Opus 复用    │
│  → SW decim 2:1 → 16kHz     │                 │  → 火山引擎 ASR → SendInput  │
│  → Opus 编码 → BLE Notify   │                 │  → 粘贴到当前窗口             │
│  按键: 按开始/再按停(切换模式)│                  │  托盘图标 + 状态悬浮窗        │
│  LCD: 蓝空闲/红录音/黄等待ASR│                  │                              │
└──────────────────────────────┘                 └──────────────────────────────┘
```

### 音频链路详解

```
Atomic Echo Base                    ESP32-S3                        PC
─────────────────    ─────────────────────────────    ──────────────────────
ES8311 codec  ──I2S──→  I2S RX @ 32kHz 16-bit    ──BLE──→  Ogg Opus mux
   │                    (BCLK=1.024MHz)                    → Volcengine ASR
   │                    ↓                                  → SendInput 粘贴
   │                    SW decim 2:1 → 16kHz
   │                    ↓
   │                    Opus encode (60ms frames)
   │                    ↓
   │                    BLE notify (audio_tx)
   │
   └── I²C (GPIO38/39) ←── ES8311 reg config
   └── PCA9557 (0x43)  ←── Power on/off
```

### BLE 协议（兼容 voicestick）

| Service/Char | UUID | 方向 | 属性 |
|---|---|---|---|
| Voice Stick Service | `8f2f0b84-6e6f-4b23-88f7-3a3ceafc5100` | — | — |
| audio_tx | `...5101` | AtomS3R → Windows | notify |
| state_tx | `...5102` | AtomS3R → Windows | notify |
| control_rx | `...5103` | Windows → AtomS3R | write without rsp |

设备名：`VS-XXXX`（基于 MAC 后 2 字节）

详见 [docs/protocol.md](docs/protocol.md)

---

## 当前状态

### ✅ 已完成/正常

| 模块 | 说明 |
|------|------|
| **BLE 广播** | NimBLE 协议栈，VS-E3F6 可见，3 个 characteristic 正常 |
| **BLE 通知** | audio_tx + state_tx 通知正常工作（handle=3/6） |
| **控制接收** | control_rx 支持 write + write_no_rsp |
| **按键交互** | GPIO41 中断，按开始/再按停（切换模式） |
| **LCD 屏幕** | GC9107 128×128，蓝色空闲/红色录音/黄色等待 |
| **背光** | LP5562 I²C 控制，可调亮度（默认 0x50/255） |
| **Opus 编码** | 28kbps VBR，60ms 帧（960 samples），优化语音信号 |
| **ES8311 I²C 通信** | I2C_NUM_1 (GPIO38/39) 总线正常，PCA9557 和 ES8311 均探测成功 |
| **ES8311 寄存器初始化** | 所有寄存器写入成功（含正确时钟分频系数） |
| **I2S 通道配置** | 32kHz 标准模式，BCLK=1.024MHz，PHILIPS 16-bit mono |
| **Windows 桌面端** | Python 版（PyQt5 + bleak）：BLE 连接/ASR/文本注入全部正常 |
| **火山引擎 ASR** | WebSocket 二进制协议（V3 大模型 API），Ogg Opus 格式，识别正常 |
| **协议栈** | BLE 音频帧 + ASR 二进制帧均已通过 Volcengine 标准格式验证 |

### ❌ 未解决（当前阻塞点）

| 问题 | 说明 |
|------|------|
| **ES8311 I2S 无数据输出** | `i2s_channel_read()` 100% 超时，ES8311 初始化成功但不输出 PCM。PCA9557 已正确上电、寄存器已配置为 ESPHome 系数表的值，但 I2S 总线无数据到达 |

---

## 踩坑记录

### 1. GC9107 屏幕偏移

**现象：** 屏幕底部花屏。

**原因：** GC9107 的内部 DRAM 是 128×160，但面板只有 128×128 可见。**可见区域起始行偏移 32**（`DISPLAY_OFFSET_Y = 32`）。原先设置 `RASET(0, 127)` 把数据写到了 RAM 的 0-127 行，但屏幕显示的是 32-159 行，底部 128-159 行是未初始化的随机数据。

**修复：** `RASET(32, 159)`，128 行像素数据写入 RAM 的正确位置，与面板对齐。

**参考：** `main/boards/atoms3r-echo-base/config.h` 中 `DISPLAY_OFFSET_Y 32`

### 2. SPI 传输可靠性

**现象：** 底部花屏（初次修复后仍有问题）。

**原因：** 逐行发送 SPI 事务导致 CS 在行间释放，GC9107 内部地址计数器偏移。

**修复（综合方案）：**
- 单次突发传输整个 framebuffer（32KB）
- `spi_device_acquire_bus()` + `SPI_TRANS_CS_KEEP_ACTIVE` 保持 CS 连续
- 16 字节 DMA 对齐（ESP32-S3 cache line）
- SPI 时钟从 40MHz 降到 20MHz
- `queue_size` 从 1 改为 2

### 3. 按键 GPIO 引脚

**现象：** 按键按下去没反应，或响应很慢。

**原因：** 官方资料标注 AtomS3R 按键在 GPIO41（`USER_BUT`），但早期测试认为 GPIO39 可用，GPIO41 无反应。**后来确认 `BOARD_BUTTON_PIN GPIO_NUM_41` 正确工作。** 曾怀疑不同批次差异或 Echo Base 冲突，实际是本项目硬件使用 GPIO41。

**当前：** 使用 `BOARD_BUTTON_PIN GPIO_NUM_41`

### 4. 按键驱动方式

**现象：** GPIO 引脚正确但仍不灵敏，有时要按多次才触发。

**原因：** 最初的轮询方式（`vTaskDelay(1)` 循环）不可靠，尤其在 FreeRTOS 任务调度下可能错过边沿。

**修复：** 改用 **GPIO 中断**（`GPIO_INTR_NEGEDGE`），ISR 中发信号量唤醒专用任务消抖（50ms）。

### 5. BLE 通知句柄

**现象：** BLE 连接成功、特征发现成功，但 notify 发不出去。

**原因：** `ble_gatts_notify_custom()` 需要的 attribute handle 始终为 0。NimBLE 的 GATT 句柄是在 **主机同步完成**（`ble_sync_cb`）后才分配的，不是在 `ble_gatts_add_svcs()` 之后。

**修复：** 将 `ble_gatts_find_chr()` 放在 `ble_sync_cb` 中调用：
```c
static void ble_sync_cb(void) {
    ble_gatts_find_chr(&s_svc_uuid.u, &s_chr_audio_tx.u, NULL, &s_audio_attr_handle);
    ble_gatts_find_chr(&s_svc_uuid.u, &s_chr_state_tx.u, NULL, &s_state_attr_handle);
    start_advertising();
}
```
正确句柄值：`audio=3 state=6`

### 6. BLE control_rx 属性

**现象：** Windows 端写 control_rx 失败。

**原因：** 原先只设置了 `BLE_GATT_CHR_F_WRITE`（带响应写），但 Windows 端使用 **WriteWithoutResponse**。缺少 `BLE_GATT_CHR_F_WRITE_NO_RSP`。

**修复：** 
```c
.flags = BLE_GATT_CHR_F_WRITE | BLE_GATT_CHR_F_WRITE_NO_RSP
```

### 7. 火山引擎 ASR 认证

**现象：** WebSocket 连上后返回 `400` 或 `missing_api_key`。

**原因：** VoiceStick 的 ASR 协议通过 **HTTP 自定义 Header** 传递 API Key，不是放在 JSON payload 中。

**修复：** 连接 WebSocket 时设置以下 Header：
```
X-Api-Key: {api_key}
X-Api-Request-Id: {uuid}
X-Api-Sequence: -1
X-Api-Resource-Id: volc.seedasr.sauc.duration
```

### 8. 火山引擎音频格式

**现象：** 服务器返回 `[Invalid audio format]`。

**原因：** V3 大模型 API 的音频格式必须为 `format: "ogg"` + `codec: "opus"`。不能使用 `raw` + `opus` 或不带格式的连接包。

**修复：** 修正 `make_start_connection()` 中的 `req_params.audio` 字段，并在连接阶段就发送完整的音频参数。

### 9. 二进制协议帧格式（已废弃）

**现象：** VoiceStick 自定义协议下，服务器返回 `declared body size does not match actual body size`。

**原因：** 在 `_pack_frame` 中，当 `session_id` 为空时也写入了 4 字节的长度字段（0）。但 C++ 原版在 `session_id` 为空时**不写入长度字段**。

**修复：** 只在 `session_id` 非空时写入 session_id 长度和内容。

**后续：** 已切换到火山引擎标准二进制协议（`[header 4B] + [body_size 4B] + [data]`），不再使用 VoiceStick 自定义协议。

### 10. 动画循环延迟

**现象：** 按按钮后屏幕要等 500ms 才变化。

**原因：** 空闲状态下动画循环 `vTaskDelay(500)` 导致状态检查延迟。

**修复：** 降到 `vTaskDelay(100)`。

### 11. LP5562 背光初始值

**现象：** 屏幕过亮。

**原因：** LP5562 亮度初始化为 `0xFF`（255 = 100%）。

**修复：** 改为 `0x50`（约 31%），更省电且舒适。

### 12. I²C 总线冲突（ES8311 vs LP5562）

**现象：** ES8311 初始化后屏幕熄灭。

**原因：** LP5562（背光）使用 I2C_NUM_0（GPIO45/0），ES8311 初始化时也尝试用 I2C_NUM_0（GPIO38/39），导致 I²C 驱动冲突：`driver_ng is not allowed`。两个设备使用不同的引脚和地址，但共享总线号。

**修复：** LP5562 保持在 I2C_NUM_0，ES8311 使用 I2C_NUM_1，各自独立总线。

### 13. ES8311 寄存器配置（核心问题）

**现象：** ES8311 I²C 初始化成功，寄存器全部写入正确，`i2s_channel_read()` 始终超时（ESP_ERR_TIMEOUT），ES8311 不输出 PCM 数据。

**尝试历史（按时间顺序）：**

| # | 方法 | 结果 |
|---|------|------|
| 1 | 从 VoiceStick 代码复制 ES8311 初始化（PLL 配置方式） | ❌ 寄存器地址猜错，写入值对不上 |
| 2 | 对照 datasheet 重新推算寄存器值 | ❌ PLL bit 没设置（0x01=0x0A → 应为 0x4A），但 BCLK 512kHz 不够 ES8311 MCLK 最低需求 |
| 3 | 将 I2S 改为 32kHz → BCLK=1.024MHz，软件 2:1 降采样 | ⚠️ 寄存器值仍然不是 ESPHome 正确值 |
| 4 | 找到 ESPHome 系数表（mclk=1024000, rate=16000），写入正确值 | ⚠️ I2C 通信 100% 成功，但 I2S 仍然 100% timeout |

**正确寄存器值（来自 ESPHome，已验证 I²C 写入成功）：**
```c
es8311_write(0x00, 0x1F);  // Reset
es8311_write(0x00, 0x00);  // Release reset
es8311_write(0x01, 0xBF);  // Clock: BCLK as MCLK, all clocks on
es8311_write(0x02, 0x20);  // pre_div=1, pre_mult=4
es8311_write(0x03, 0x10);  // ADC OSR: 64×fs
es8311_write(0x04, 0x20);  // DAC OSR: 128×fs
es8311_write(0x05, 0x00);  // adc_div=1, dac_div=1
es8311_write(0x06, 0x04);  // BCLK divider=4
es8311_write(0x07, 0x00);  // LRCK high
es8311_write(0x08, 0xFF);  // LRCK low
es8311_write(0x09, 3<<2);  // SDPIN 16-bit
es8311_write(0x0A, 3<<2);  // SDPOUT 16-bit
es8311_write(0x0D, 0x01);  // Power up
es8311_write(0x0E, 0x02);  // Enable PGA + ADC mod
es8311_write(0x12, 0x00);  // Enable DAC
es8311_write(0x13, 0x10);  // Output to HP drive
es8311_write(0x14, 0x1A);  // Analog mic enable
es8311_write(0x15, 0x00);
es8311_write(0x16, 0x00);
es8311_write(0x17, 0xC8);  // ADC volume ~6dB
es8311_write(0x1C, 0x6A);  // HPF on, EQ bypass
es8311_write(0x37, 0x08);  // DAC EQ bypass
es8311_write(0x32, 0xBF);  // DAC 0dB
es8311_write(0x00, 0x80);  // Power on
```

### 14. 板载 PDM 麦克风（已弃用）

**现象：** 所有 PDM 麦克风方案都无法持续产出有效音频。

**尝试总结：**

| 尝试 | 方法 | 结果 |
|------|------|------|
| I2S PDM RAW (sample_rate=2048000) | RAW 模式，MCLK=2.048M×128=262MHz 超限 | ⚠️ 偶有数据但帧率极低 |
| I2S PDM RAW (sample_rate=16000) | RAW 模式+音频速率 | ❌ INVALID_STATE |
| I2S PDM 标准模式 (16000) | 硬件 PDM→PCM | ❌ 100% timeout |
| I2S 标准+软件 CIC (BCLK=2.048M) | I2S标准+I2S_DIN读PDM比特流→软件 CIC 128:1 | ❌ raw[] 全 0x0000 |
| I2S 标准+PHILIPS | 换格式 | ❌ 全 0x0000 |
| I2S 标准+MSB+BCLK翻转+LSB提取 | 调采样沿+比特序 | ⚠️ 前720ms有效后变静音 |
| 16-bit 直接降采样 | 抛弃 CIC，直接用 I2S 16-bit 值 | ❌ 全部静音帧 |

**结论：** ESP32-S3 v0.2 的 I2S PDM 外设无法可靠工作。改用 Atomic Echo Base（ES8311 codec）走标准 I2S PCM 路线。

---

## 当前困惑与待解决问题

### ES8311 I2S 无数据 — 可能原因

寄存器值已通过 ESPHome 系数表验证正确，I²C 写入确认成功，但 ES8311 始终不向 I2S 总线输出 PCM 数据。可能原因：

1. **初始化顺序问题** — I²C 寄存器写入后有依赖顺序（如 power up 必须在 clock config 之后足够延迟），而当前没有加足够 delay
2. **BCLK 时钟源问题** — ES8311 在从机模式下依赖 ESP32-S3 I2S master 提供 BCLK。BCLK=1.024MHz 刚好处于 ES8311 工作边界，可能需要更高 BCLK 或外部晶振
3. **I2S 格式不匹配** — ES8311 SDPIN/SDPOUT 配置为 16-bit I2S，但 I2S 驱动配置为 Philips 格式可能有细微差异（如延迟位、字长）
4. **PCA9557 电源控制不完整** — 可能还缺其他 GPIO 控制（如 reset 引脚、MCLK 使能）
5. **MCLK 引脚未连接** — ES8311 的 MCLK 引脚在 Atomic Echo Base 上可能悬空，需要通过寄存器 0x01 的 BIT7 选择 BCLK 作为时钟源 — 已设置但仍需确认
6. **ES8311 处于错误的工作模式** — 寄存器 0x00 的 BIT6 控制 32kHz 模式，可能需要特殊设置

---

## 参考链接

| 项目 | 链接 | 用途 |
|------|------|------|
| VoiceStick | https://github.com/78/voicestick | BLE 协议规范 + Windows 桌面端参考 |
| XiaoZhi ESP32 | https://github.com/78/xiaozhi-esp32 | AtomS3R 参考固件（GC9107 + ES8311，关键参考） |
| M5AtomS3R 文档 | https://docs.m5stack.com/en/core/AtomS3R | 硬件规格 |
| M5AtomS3R AI Chatbot | https://docs.m5stack.com/en/core/AtomS3R-AI%20Chatbot | AI Chatbot 套件文档 |
| M5 Echo Base | https://docs.m5stack.com/en/base/Atomic%20Echo%20Base | Echo Base 规格（含 ES8311） |
| ESP-IDF | https://github.com/espressif/esp-idf | ESP32 开发框架 v5.5 |
| 火山引擎语音识别 | https://www.volcengine.com/docs/6561/1354869 | ASR 服务 |
| ES8311 Datasheet | https://github.com/lewisxhe/AudioCodec-XN2971/blob/master/doc/ES8311_datasheet.pdf | ES8311 寄存器手册 |
| ESPHome ES8311 | https://github.com/esphome/esphome/blob/dev/esphome/components/es8311/es8311.cpp | ESPHome ES8311 驱动系数表参考 |
