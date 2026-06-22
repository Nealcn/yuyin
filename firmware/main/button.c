#include <string.h>
#include "esp_log.h"
#include "esp_check.h"
#include "esp_timer.h"
#include "driver/gpio.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/timers.h"

#include "button.h"
#include "board_config.h"

static const char *TAG = "button";

// Double-click detection window (ms)
#define DOUBLE_CLICK_WINDOW_MS  300
// Long press threshold (ms)
#define LONG_PRESS_THRESHOLD_MS 3000
// Debounce time (ms)
#define DEBOUNCE_MS             30

static button_cb_t s_button_cb = NULL;
static bool s_is_pressed = false;
static int64_t s_press_start_us = 0;
static bool s_awaiting_double_click = false; // waiting for second press after release
static TimerHandle_t s_double_click_timer = NULL;
static TaskHandle_t s_button_task = NULL;

static void double_click_timer_cb(TimerHandle_t xTimer)
{
    (void)xTimer;
    // Timer expired without a second click -> this was a single press
    s_awaiting_double_click = false;
    if (s_button_cb) {
        s_button_cb(BUTTON_EVENT_PRESS_UP, 0);
    }
}

static void button_monitor_task(void *arg)
{
    (void)arg;
    ESP_LOGI(TAG, "button monitor task started");

    // Configure GPIO
    gpio_config_t io_conf = {
        .pin_bit_mask = 1ULL << BOARD_BUTTON_PIN,
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    gpio_config(&io_conf);

    bool prev_level = true; // idle high (pull-up)
    int stable_ticks = 0;

    while (1) {
        bool level = gpio_get_level(BOARD_BUTTON_PIN);

        if (level != prev_level) {
            stable_ticks = 0;
            prev_level = level;
            vTaskDelay(pdMS_TO_TICKS(1));
            continue;
        }

        if (stable_ticks < DEBOUNCE_MS) {
            stable_ticks++;
            vTaskDelay(pdMS_TO_TICKS(1));
            continue;
        }

        // Stable state achieved
        bool pressed = (level == 0); // active low

        if (pressed && !s_is_pressed) {
            // Press detected
            s_is_pressed = true;
            s_press_start_us = esp_timer_get_time();

            // Cancel any pending double-click detection
            if (s_awaiting_double_click) {
                xTimerStop(s_double_click_timer, 0);
                s_awaiting_double_click = false;
                // Second press within window -> double click
                ESP_LOGI(TAG, "double click detected");
                if (s_button_cb) {
                    s_button_cb(BUTTON_EVENT_DOUBLE_CLICK, 0);
                }
                stable_ticks = 0;
                vTaskDelay(pdMS_TO_TICKS(1));
                continue;
            }

            // First press
            if (s_button_cb) {
                s_button_cb(BUTTON_EVENT_PRESS_DOWN, 0);
            }

        } else if (!pressed && s_is_pressed) {
            // Release detected
            s_is_pressed = false;
            int64_t duration_us = esp_timer_get_time() - s_press_start_us;
            uint32_t duration_ms = (uint32_t)(duration_us / 1000);

            // Check for long press
            if (duration_ms >= LONG_PRESS_THRESHOLD_MS) {
                ESP_LOGI(TAG, "long press: %u ms", duration_ms);
                if (s_button_cb) {
                    s_button_cb(BUTTON_EVENT_LONG_PRESS, duration_ms);
                }
                stable_ticks = 0;
                vTaskDelay(pdMS_TO_TICKS(1));
                continue;
            }

            // Start double-click detection window
            s_awaiting_double_click = true;
            xTimerReset(s_double_click_timer, 0);

            ESP_LOGD(TAG, "release detected, waiting for double-click (%u ms)", duration_ms);
        }

        vTaskDelay(pdMS_TO_TICKS(1));
    }
}

esp_err_t button_init(button_cb_t cb)
{
    ESP_RETURN_ON_FALSE(cb != NULL, ESP_ERR_INVALID_ARG, TAG, "callback required");

    s_button_cb = cb;

    // Create double-click timer
    s_double_click_timer = xTimerCreate("btn_dblclk", pdMS_TO_TICKS(DOUBLE_CLICK_WINDOW_MS),
                                         pdFALSE, NULL, double_click_timer_cb);
    ESP_RETURN_ON_FALSE(s_double_click_timer, ESP_ERR_NO_MEM, TAG, "create timer failed");

    // Create button monitor task
    BaseType_t ok = xTaskCreate(button_monitor_task, "btn_mon", 3072, NULL, 7, &s_button_task);
    ESP_RETURN_ON_FALSE(ok == pdPASS, ESP_ERR_NO_MEM, TAG, "create task failed");

    ESP_LOGI(TAG, "button initialized on GPIO%d", BOARD_BUTTON_PIN);
    return ESP_OK;
}

uint32_t button_get_press_duration_ms(void)
{
    if (!s_is_pressed) return 0;
    return (uint32_t)((esp_timer_get_time() - s_press_start_us) / 1000);
}

bool button_is_pressed(void)
{
    return s_is_pressed;
}

void button_deinit(void)
{
    if (s_button_task) {
        vTaskDelete(s_button_task);
        s_button_task = NULL;
    }
    if (s_double_click_timer) {
        xTimerDelete(s_double_click_timer, 0);
        s_double_click_timer = NULL;
    }
    s_button_cb = NULL;
    s_is_pressed = false;
    ESP_LOGI(TAG, "button deinitialized");
}
