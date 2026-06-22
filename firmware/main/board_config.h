#pragma once

#include <stdint.h>

// ============================================================
// M5AtomS3 Board Pin Definitions
// Reference: https://docs.m5stack.com/en/core/AtomS3
// ============================================================

// --- Built-in PDM Microphone (MSM261DCM) ---
// ESP32-S3 I2S PDM RX mode
#define BOARD_PDM_CLK_PIN       GPIO_NUM_1   // I2S PDM clock output to mic
#define BOARD_PDM_DATA_PIN      GPIO_NUM_2   // I2S PDM data input from mic

// --- User Button ---
// Single button, active low (pull-up internally)
#define BOARD_BUTTON_PIN        GPIO_NUM_39

// --- RGB LED Matrix (5x5 SK6812) ---
// NeoPixel-compatible single-wire data line
#define BOARD_RGB_LED_PIN       GPIO_NUM_35
#define BOARD_RGB_LED_COUNT     25           // 5x5 matrix

// --- USB / Power ---
// AtomS3 is USB-powered only; no battery/PMIC
// USB-C VBUS detect (if needed)
#define BOARD_USB_DETECT_PIN    GPIO_NUM_0   // USB detect (when connected)

// --- Audio Configuration ---
#define BOARD_AUDIO_SAMPLE_RATE 16000        // 16 kHz for ASR
#define BOARD_AUDIO_BITS        16           // 16-bit PCM
#define BOARD_AUDIO_CHANNELS    1            // Mono
#define BOARD_AUDIO_FRAME_MS    60           // 60 ms per Opus frame
#define BOARD_AUDIO_FRAME_SAMPLES \
    (BOARD_AUDIO_SAMPLE_RATE * BOARD_AUDIO_FRAME_MS / 1000)  // 960 samples

// --- PDM Clock Configuration ---
// PDM_CLK = sample_rate * PDM_FREQ_DIV
// For 16 kHz output with 80 divider: 16k * 80 = 1.28 MHz PDM clock
#define BOARD_PDM_CLK_DIV       80

// --- BLE Configuration ---
#define BOARD_BLE_DEVICE_PREFIX "VS-"        // Must match voicestick prefix
