---
name: button-gpio-pin
description: M5AtomS3R AI Chatbot 按键实际使用 GPIO39 而非官方标注的 GPIO41
metadata:
  type: reference
---

M5AtomS3R AI Chatbot 的按键实测为 **GPIO39**（低电平有效，内部上拉），
官方文档标注为 GPIO41 但在本硬件上不响应。

可能原因：
- 不同硬件批次有差异
- AI Chatbot 套件的 Echo Base 底板可能将按键信号路由到了 GPIO39
- GPIO41 在 Echo Base 上可能被其他电路拉低

**配置位置：** `firmware/main/board_config.h` 中的 `BOARD_BUTTON_PIN`

参考官方文档：https://docs.m5stack.com/en/core/AtomS3R-AI%20Chatbot
（但实际测试 GPIO39 可用，GPIO41 无响应）
