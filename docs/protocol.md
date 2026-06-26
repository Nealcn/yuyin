# Voice Stick BLE 协议 — AtomS3R 实现

与 [78/voicestick](https://github.com/78/voicestick/blob/main/docs/protocol.md) 协议兼容。

## BLE GATT

设备名：`VS-XXXX`（基于 MAC 地址后 2 字节）

| Service/Char | UUID | 方向 | 属性 |
|---|---|---|---|
| Voice Stick Service | `8f2f0b84-6e6f-4b23-88f7-3a3ceafc5100` | — | — |
| audio_tx | `...5101` | AtomS3R → Windows | notify |
| state_tx | `...5102` | AtomS3R → Windows | notify |
| control_rx | `...5103` | Windows → AtomS3R | write without response |

协议栈：**NimBLE**（ESP-IDF v5.5 内置）

## 音频帧

小端序打包结构：

```c
struct AudioBleFrame {
  uint8_t  version;    // 1
  uint8_t  type;       // 0x01
  uint16_t header_len; // 16
  uint32_t session_id;
  uint32_t seq;
  uint8_t  flags;      // bit0=start, bit1=end
  uint8_t  reserved;
  uint16_t payload_len;
  uint8_t  payload[];  // Opus packet
}
```

- 60ms Opus 帧，16kHz / 16-bit 单声道
- 码率：~28 kbps VBR
- 编码器：`OPUS_APPLICATION_AUDIO`，复杂度 5

## 状态事件（JSON，通过 state_tx）

按键事件（单按键映射为 "primary"，双击映射为 "secondary"）：

```json
{"event":"button_down","button":"primary","session_id":1234}
{"event":"button_up","button":"primary","duration_ms":620,"session_id":1234}
{"event":"button_down","button":"secondary","session_id":0}
{"event":"button_up","button":"secondary","duration_ms":90,"session_id":0}
```

## 控制命令（JSON，从 Windows 通过 control_rx 写入）

```json
{"event":"ui_state","state":"ready","text":""}
{"event":"ui_state","state":"recording","text":""}
{"event":"ui_state","state":"thinking","text":"partial text"}
{"event":"ui_state","state":"pending_confirmation","text":"final text"}
{"event":"ui_state","state":"error","text":"ASR timeout"}
```

## 与原始 voicestick 的差异

| 项目 | StickS3（原始） | AtomS3R（本实现） |
|------|----------------|-------------------|
| 主控 | ESP32-S3 | ESP32-S3-PICO |
| PSRAM | 无 | **8MB** |
| 音频输入 | ES8311 I2S 编解码器 | 板载 **PDM 麦克风**（MSM261DCM） |
| 显示 | ST7789 LCD 240×135 | **GC9107** 128×128 彩色 IPS LCD |
| 背光 | GPIO 直控 | **LP5562** I²C 驱动 |
| 按键 | 正面+侧面 2 个 | **单按键**（双击=取消） |
| 电池 | AXP2101 PMIC | USB 供电（无电池） |
| BLE 协议栈 | Bluedroid | **NimBLE** |
| OTA | 支持 | 未实现 |
