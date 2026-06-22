#pragma once

#include <stdint.h>
#include <stdbool.h>
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief LED state indicators
 */
typedef enum {
    LED_STATE_IDLE,              // Blue (breathing when not connected, solid when connected)
    LED_STATE_RECORDING,         // Red wave animation
    LED_STATE_THINKING,          // Yellow spinning
    LED_STATE_CONFIRMED,         // Green flash x2 then back to idle
    LED_STATE_CANCELLED,         // Red X flash then back to idle
    LED_STATE_ERROR,             // Red blinking
    LED_STATE_OTA,               // OTA update progress
} led_state_t;

/**
 * @brief Initialize the 5x5 RGB LED matrix (SK6812 on GPIO35)
 * Uses RMT peripheral for NeoPixel protocol timing
 */
esp_err_t led_matrix_init(void);

/**
 * @brief Set current LED state (triggers animation)
 */
void led_matrix_set_state(led_state_t state);

/**
 * @brief Get current LED state
 */
led_state_t led_matrix_get_state(void);

/**
 * @brief Set brightness level (0-255)
 */
void led_matrix_set_brightness(uint8_t brightness);

/**
 * @brief Set OTA progress percentage (0-100)
 */
void led_matrix_set_ota_progress(uint8_t percent);

/**
 * @brief Deinitialize
 */
void led_matrix_deinit(void);

#ifdef __cplusplus
}
#endif
