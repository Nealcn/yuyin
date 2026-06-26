# M5AtomS3R Voice Stick - PDM 麦克风调试记录

## 概述
尝试使用 AtomS3R 板载 PDM 麦克风（MSM381A3729H9BPC）进行音频采集 → Opus 编码 → BLE 传输 → 火山引擎 ASR 识别。

## 硬件信息
- **主控**: ESP32-S3-PICO-1 (revision v0.2)
- **PDM 麦克风**: MSM381A3729H9BPC（板载）
- **当前引脚配置**: PDM_CLK=GPIO1, PDM_DATA=GPIO2
- **时钟**: 40MHz 晶振, 240MHz CPU, PSRAM 8MB

---

## 尝试 1: I2S PDM RAW 模式 (原始配置)
**文件**: `pdm_mic.c` - RAW 模式, `sample_rate_hz=2048000`, `mclk_multiple=128`, `bclk_div=2`
**结果**: ⚠️ 部分成功

- I2S 大量 timeout（10次/秒），但偶有成功读取
- PCM 帧约 510ms 才产出一帧（正常应为 60ms）
- Opus 帧大小正常变化（85, 67, 65, 67, 56, 49...），音频内容有效
- 失败原因: MCLK = 2.048MHz × 128 = **262 MHz** 超出 ESP32-S3 时钟源范围（~160MHz），导致 PLL 不稳定
- 后续尝试增加 `bclk_div=32` 时 `i2s_channel_enable()` 返回 `ESP_ERR_INVALID_STATE`

## 尝试 2: I2S PDM RAW 模式 + sample_rate=16000
**改动**: `sample_rate_hz=BOARD_AUDIO_SAMPLE_RATE(16000)`
**结果**: ❌ 失败

- `i2s_channel_enable()` 返回 `ESP_ERR_INVALID_STATE`
- MCLK = 16000 × 128 = 2.048 MHz（有效），但 RAW 模式下 `sample_rate_hz` 需要 PDM 位时钟频率
- 配置不兼容，通道无法使能

## 尝试 3: I2S PDM 标准模式（硬件 PDM→PCM）
**改动**: 改用 `I2S_PDM_RX_SLOT_DEFAULT_CONFIG`（非 RAW），`sample_rate_hz=16000`, `mclk_multiple=256`
**结果**: ❌ 100% timeout

- MCLK = 4.096 MHz（有效），BCLK = 2.048 MHz
- 但 I2S PDM 标准模式的硬件 PDM→PCM 滤波器始终超时，不产出数据
- 可能是 ESP32-S3 v0.2 芯片版本的 I2S PDM 外设问题

## 尝试 4: 修正 PDM 引脚 GPIO38/GPIO39
**依据**: M5Unified 文档显示 PDM_CLK=GPIO39, PDM_DAT=GPIO38
**改动**: `board_config.h` 中 PDM 引脚改为 GPIO38/39，PDM 标准模式
**结果**: ❌ 100% timeout

- GPIO38/39 在所有 I2S 模式下均无数据
- 后续怀疑 M5Unified 的引脚信息可能对应不同硬件版本
- 确认 GPIO1/GPIO2 才是正确的引脚（RAW 模式出过数据）

## 尝试 5: I2S 标准模式 + 软件 CIC 滤波
**核心思路**: 用 I2S 标准 RX 模式代替 PDM 外设，BCLK=2.048MHz 作 PDM 时钟，软件 CIC 128:1 解 PDM→PCM
**改动**: 
- `I2S_STD_CLK_DEFAULT_CONFIG(128000)` → BCLK = 2.048MHz
- `I2S_STD_PCM_SLOT_DEFAULT_CONFIG`（PCM 短帧同步格式）
- WS = GPIO3, BCLK = GPIO1, DIN = GPIO2
- MCLK = 128000 × 256 = 32.768 MHz ✅
**结果**: ❌ raw[0..7] 全 0x0000

- 调试日志显示所有 I2S 16-bit 字都是 0x0000
- DIN(GPIO2) 始终为低电平，I2S 没有捕获到任何 PDM 数据
- I2S 标准模式的帧同步格式与 PDM 连续比特流不匹配

## 尝试 6: I2S 标准模式 + PHILIPS 格式
**改动**: `I2S_STD_PHILIPS_SLOT_DEFAULT_CONFIG`（标准 I2S 格式）
**结果**: ❌ raw[0..7] 全 0x0000

## 尝试 7: I2S 标准模式 + MSB 格式 + BCLK 翻转 + LSB 提取
**改动**:
- `I2S_STD_MSB_SLOT_DEFAULT_CONFIG`
- `.invert_flags = { .bclk_inv = true }` — 翻转 BCLK 采样沿
- CIC 拆位改为 LSB 优先
**结果**: ⚠️ 部分成功

- 前 12 帧（~720ms）Opus 帧大小变化（85, 67, 65, 67...）
- 之后全变 17 字节静音帧
- 说明音频被捕获到了，但 720ms 后信号衰减为常数
- ASR 返回空结果

## 尝试 8: 释放 UART0 + PDM 标准模式
**改动**: `uart_driver_delete(UART_NUM_0)` 释放 GPIO1/3
**结果**: ❌ 100% timeout

## 尝试 9: 直接 16-bit PCM 降采样
**改动**: 抛弃 CIC 拆位，直接用 I2S 16-bit 值作 PCM，8:1 平均降采样
**结果**: ❌ 全部 17 字节静音帧

---

## 结论
AtomS3R 板载 PDM 麦克风的 I2S PDM 外设在 ESP32-S3 v0.2 上无法可靠工作。所有尝试中只有 **尝试 1（PDM RAW 模式）** 产出了有效的音频数据，但因 MCLK 超限导致帧率极低。

**改用 Atomic Echo Base（ES8311 I2S 编解码器）**，通过标准 I2S 接口走 PCM 音频，可绕开 PDM 外设问题。
