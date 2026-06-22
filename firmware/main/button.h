#pragma once

#include <stdint.h>
#include <stdbool.h>
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Button event types
 */
typedef enum {
    BUTTON_EVENT_PRESS_DOWN,     // Button pressed
    BUTTON_EVENT_PRESS_UP,       // Button released
    BUTTON_EVENT_DOUBLE_CLICK,   // Double-click detected
    BUTTON_EVENT_LONG_PRESS,     // Held for > 3 seconds (reserved)
} button_event_t;

/**
 * @brief Button callback
 */
typedef void (*button_cb_t)(button_event_t event, uint32_t duration_ms);

/**
 * @brief Initialize button (GPIO39, active low)
 * @param cb Callback for button events
 */
esp_err_t button_init(button_cb_t cb);

/**
 * @brief Get current press duration (ms) if button is held
 */
uint32_t button_get_press_duration_ms(void);

/**
 * @brief Check if button is currently pressed
 */
bool button_is_pressed(void);

/**
 * @brief Deinitialize
 */
void button_deinit(void);

#ifdef __cplusplus
}
#endif
