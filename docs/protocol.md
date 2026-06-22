# Voice Stick BLE Protocol — AtomS3 Variant

Fully compatible with [78/voicestick protocol](https://github.com/78/voicestick/blob/main/docs/protocol.md).

## GATT Service

Device name: `VS-XXXX` (last 2 bytes of MAC)

| Service/Char | UUID | Direction | Properties |
|---|---|---|---|
| Voice Stick Service | `8f2f0b84-6e6f-4b23-88f7-3a3ceafc5100` | — | — |
| audio_tx | `...5101` | AtomS3 → Windows | notify |
| state_tx | `...5102` | AtomS3 → Windows | notify |
| control_rx | `...5103` | Windows → AtomS3 | write without response |

## Audio Frame

Packed little-endian binary:

```
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

- 60ms Opus frames at 16 kHz / 16-bit mono
- Bitrate target: ~28 kbps (VBR)

## State Events (JSON over state_tx)

Button events (single button maps to "primary"):

```json
{"event":"button_down","button":"primary","session_id":1234}
{"event":"button_up","button":"primary","duration_ms":620,"session_id":1234}
{"event":"button_down","button":"secondary","session_id":0}
{"event":"button_up","button":"secondary","duration_ms":90,"session_id":0}
```

Double-click sends as secondary button click (for cancel action).

## Control Events (JSON over control_rx, from Windows)

```json
{"event":"ui_state","state":"ready","text":""}
{"event":"ui_state","state":"recording","text":""}
{"event":"ui_state","state":"thinking","text":"partial text"}
{"event":"ui_state","state":"pending_confirmation","text":"final text"}
{"event":"ui_state","state":"error","text":"ASR timeout"}
```

## Differences from voicestick

| Feature | StickS3 (original) | AtomS3 (this project) |
|---|---|---|
| Audio source | ES8311 I2S codec | MSM261DCM PDM mic (built-in) |
| Display | ST7789 LCD (240×135) | 5×5 RGB LED matrix |
| Buttons | Front (GPIO11) + Side (GPIO12) | Single button (GPIO39) |
| Double-click | Separate physical button | Double-click on single button |
| Battery | AXP2101 PMIC | USB powered only |
| OTA | Supported | Future |
