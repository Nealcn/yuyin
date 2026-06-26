/**
 * @file button.c
 * @brief GPIO falling-edge ISR → wakes dedicated task for debounce + callback.
 *        Never runs heavy code inside Tmr Svc context.
 */
#include "esp_log.h"
#include "esp_check.h"
#include "driver/gpio.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"

#include "button.h"
#include "board_config.h"

static const char *TAG = "button";
static button_cb_t s_cb = NULL;
static SemaphoreHandle_t s_sem = NULL;
static TaskHandle_t s_task = NULL;

static void IRAM_ATTR gpio_isr_handler(void *arg)
{
    (void)arg;
    BaseType_t wake = pdFALSE;
    xSemaphoreGiveFromISR(s_sem, &wake);
    if (wake) portYIELD_FROM_ISR();
}

static void button_task(void *arg)
{
    (void)arg;

    // Configure GPIO falling-edge interrupt
    gpio_config_t cfg = {
        .pin_bit_mask = 1ULL << BOARD_BUTTON_PIN,
        .mode         = GPIO_MODE_INPUT,
        .pull_up_en   = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type    = GPIO_INTR_NEGEDGE,
    };
    gpio_config(&cfg);
    gpio_install_isr_service(0);
    gpio_isr_handler_add(BOARD_BUTTON_PIN, gpio_isr_handler, NULL);

    ESP_LOGI(TAG, "ready GPIO%d", BOARD_BUTTON_PIN);

    while (1) {
        // Wait for ISR to signal a potential press
        xSemaphoreTake(s_sem, portMAX_DELAY);

        // Debounce: wait 50 ms then re-check
        vTaskDelay(pdMS_TO_TICKS(50));

        if (gpio_get_level(BOARD_BUTTON_PIN) == 0) {
            // Still LOW — confirmed press
            ESP_LOGI(TAG, "PRESS_DOWN");
            if (s_cb) s_cb(BUTTON_EVENT_PRESS_DOWN, 0);
        }

        // Consume any stale semaphore counts that accumulated during debounce
        while (xSemaphoreTake(s_sem, 0) == pdTRUE);
    }
}

esp_err_t button_init(button_cb_t cb)
{
    ESP_RETURN_ON_FALSE(cb, ESP_ERR_INVALID_ARG, TAG, "cb required");
    s_cb = cb;

    s_sem = xSemaphoreCreateBinary();
    ESP_RETURN_ON_FALSE(s_sem, ESP_ERR_NO_MEM, TAG, "semaphore");

    BaseType_t ok = xTaskCreate(button_task, "btn", 3072, NULL, 7, &s_task);
    ESP_RETURN_ON_FALSE(ok == pdPASS, ESP_ERR_NO_MEM, TAG, "task");

    return ESP_OK;
}

uint32_t button_get_press_duration_ms(void) { return 0; }
bool    button_is_pressed(void)            { return false; }
void    button_deinit(void)                 { s_cb = NULL; }
