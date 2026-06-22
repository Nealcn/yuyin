#include <string.h>
#include <stdlib.h>
#include "esp_log.h"
#include "esp_check.h"
#include "driver/rmt_tx.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"

#include "led_matrix.h"
#include "board_config.h"

static const char *TAG = "led";

#define RMT_RESOLUTION_HZ   80000000

static rmt_channel_handle_t s_rmt_chan = NULL;
static rmt_encoder_handle_t s_bytes_encoder = NULL;
static rmt_encoder_handle_t s_copy_encoder = NULL;
static SemaphoreHandle_t s_led_mutex = NULL;

static led_state_t s_current_state = LED_STATE_IDLE;
static uint8_t s_brightness = 220;
static uint8_t s_ota_percent = 0;
static TaskHandle_t s_anim_task = NULL;

// Color buffer for 25 LEDs in GRB order (SK6812 format)
typedef struct __attribute__((packed)) {
    uint8_t g;
    uint8_t r;
    uint8_t b;
} grb_t;

static grb_t s_leds[BOARD_RGB_LED_COUNT];
static const int MATRIX_SIZE = 5;

// Reset code symbol (low for 50us)
static rmt_symbol_word_t s_reset_code;

// Pixel mapping: zigzag row-major for 5x5 matrix
static int xy_to_index(int x, int y)
{
    if (y % 2 == 0)
        return y * MATRIX_SIZE + x;
    else
        return y * MATRIX_SIZE + (MATRIX_SIZE - 1 - x);
}

static void clear_leds(void)
{
    memset(s_leds, 0, sizeof(s_leds));
}

static void set_pixel(int x, int y, uint8_t r, uint8_t g, uint8_t b)
{
    if (x < 0 || x >= MATRIX_SIZE || y < 0 || y >= MATRIX_SIZE) return;
    int idx = xy_to_index(x, y);
    s_leds[idx].r = (r * s_brightness) / 255;
    s_leds[idx].g = (g * s_brightness) / 255;
    s_leds[idx].b = (b * s_brightness) / 255;
}

static void set_all_pixels(uint8_t r, uint8_t g, uint8_t b)
{
    for (int y = 0; y < MATRIX_SIZE; y++)
        for (int x = 0; x < MATRIX_SIZE; x++)
            set_pixel(x, y, r, g, b);
}

// ---- Flush LED data to hardware using RMT bytes encoder ----
static esp_err_t led_matrix_flush(void)
{
    if (!s_rmt_chan) return ESP_ERR_INVALID_STATE;

    // Transmit GRB data via bytes encoder (handles bit-level timing)
    esp_err_t err = rmt_transmit(s_rmt_chan, s_bytes_encoder, s_leds,
                                  sizeof(grb_t) * BOARD_RGB_LED_COUNT,
                                  &(rmt_transmit_config_t){.loop_count = 0, .flags.eot_level = 0});
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "rmt_transmit data failed: %s", esp_err_to_name(err));
        return err;
    }

    // Transmit reset code via copy encoder (low pulse for 50us)
    err = rmt_transmit(s_rmt_chan, s_copy_encoder, &s_reset_code,
                        sizeof(s_reset_code),
                        &(rmt_transmit_config_t){.loop_count = 0, .flags.eot_level = 0});
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "rmt_transmit reset failed: %s", esp_err_to_name(err));
    }
    return err;
}

// ---- Animation patterns ----
static void anim_idle(void)
{
    static uint8_t phase = 0;
    phase = (phase + 1) % 128;
    uint8_t brightness = (phase < 64) ? phase * 2 : (128 - phase) * 2;
    clear_leds();
    set_pixel(2, 2, 0, 0, brightness);
    set_pixel(1, 2, 0, 0, brightness / 3);
    set_pixel(3, 2, 0, 0, brightness / 3);
    set_pixel(2, 1, 0, 0, brightness / 3);
    set_pixel(2, 3, 0, 0, brightness / 3);
}

static void anim_recording(void)
{
    static uint8_t phase = 0;
    phase = (phase + 1) % MATRIX_SIZE;
    clear_leds();
    for (int x = 0; x < MATRIX_SIZE; x++) {
        int y = (phase + x) % MATRIX_SIZE;
        set_pixel(x, y, 80, 0, 0);
        set_pixel(x, (y + 1) % MATRIX_SIZE, 40, 0, 0);
    }
}

static void anim_thinking(void)
{
    static uint8_t phase = 0;
    phase = (phase + 1) % (MATRIX_SIZE * 2);
    clear_leds();
    for (int i = 0; i < MATRIX_SIZE; i++) {
        int x = (phase + i) % MATRIX_SIZE;
        set_pixel(x, i, 80, 60, 0);
        set_pixel(x, MATRIX_SIZE - 1 - i, 40, 30, 0);
    }
}

static void anim_confirmed(void)
{
    static int flash_count = 0;
    if (flash_count < 4) {
        if (flash_count % 2 == 0) set_all_pixels(0, 80, 0);
        else clear_leds();
        flash_count++;
    } else {
        flash_count = 0;
        led_matrix_set_state(LED_STATE_IDLE);
    }
}

static void anim_cancelled(void)
{
    static int flash_count = 0;
    if (flash_count < 4) {
        if (flash_count % 2 == 0) {
            clear_leds();
            for (int i = 0; i < MATRIX_SIZE; i++) {
                set_pixel(i, i, 80, 0, 0);
                set_pixel(i, MATRIX_SIZE - 1 - i, 80, 0, 0);
            }
        } else clear_leds();
        flash_count++;
    } else {
        flash_count = 0;
        led_matrix_set_state(LED_STATE_IDLE);
    }
}

static void anim_error(void)
{
    static bool on = false;
    on = !on;
    if (on) set_all_pixels(80, 0, 0);
    else clear_leds();
}

static void anim_ota(void)
{
    clear_leds();
    int total = BOARD_RGB_LED_COUNT;
    int lit = (s_ota_percent * total) / 100;
    for (int i = 0; i < lit && i < total; i++) {
        set_pixel(i % MATRIX_SIZE, i / MATRIX_SIZE, 0, 0, 60);
    }
}

static void led_animation_task(void *arg)
{
    ESP_LOGI(TAG, "animation task started");
    TickType_t delay_ticks = pdMS_TO_TICKS(80);

    while (1) {
        xSemaphoreTake(s_led_mutex, portMAX_DELAY);
        led_state_t state = s_current_state;
        xSemaphoreGive(s_led_mutex);

        switch (state) {
        case LED_STATE_IDLE:       anim_idle();      delay_ticks = pdMS_TO_TICKS(40);   break;
        case LED_STATE_RECORDING:  anim_recording(); delay_ticks = pdMS_TO_TICKS(100);  break;
        case LED_STATE_THINKING:   anim_thinking();  delay_ticks = pdMS_TO_TICKS(80);   break;
        case LED_STATE_CONFIRMED:  anim_confirmed(); delay_ticks = pdMS_TO_TICKS(200);  break;
        case LED_STATE_CANCELLED:  anim_cancelled(); delay_ticks = pdMS_TO_TICKS(200);  break;
        case LED_STATE_ERROR:      anim_error();     delay_ticks = pdMS_TO_TICKS(500);  break;
        case LED_STATE_OTA:        anim_ota();       delay_ticks = pdMS_TO_TICKS(100);  break;
        }

        led_matrix_flush();
        vTaskDelay(delay_ticks);
    }
}

// ---- Public API ----

esp_err_t led_matrix_init(void)
{
    s_led_mutex = xSemaphoreCreateMutex();
    ESP_RETURN_ON_FALSE(s_led_mutex, ESP_ERR_NO_MEM, TAG, "create mutex failed");

    // Install RMT TX channel
    rmt_tx_channel_config_t rmt_cfg = {
        .gpio_num = BOARD_RGB_LED_PIN,
        .clk_src = RMT_CLK_SRC_DEFAULT,
        .resolution_hz = RMT_RESOLUTION_HZ,
        .mem_block_symbols = 256,
        .trans_queue_depth = 4,
        .flags.invert_out = false,
        .flags.with_dma = false,
    };
    ESP_RETURN_ON_ERROR(rmt_new_tx_channel(&rmt_cfg, &s_rmt_chan),
                        TAG, "create RMT TX channel failed");
    ESP_RETURN_ON_ERROR(rmt_enable(s_rmt_chan), TAG, "enable RMT channel failed");

    // Bytes encoder for GRB data (WS2812 timing, compatible with SK6812)
    rmt_bytes_encoder_config_t bytes_cfg = {
        .bit0 = {
            .level0 = 1,
            .duration0 = 0.3 * RMT_RESOLUTION_HZ / 1000000,  // T0H = 0.3us
            .level1 = 0,
            .duration1 = 0.9 * RMT_RESOLUTION_HZ / 1000000,  // T0L = 0.9us
        },
        .bit1 = {
            .level0 = 1,
            .duration0 = 0.9 * RMT_RESOLUTION_HZ / 1000000,  // T1H = 0.9us
            .level1 = 0,
            .duration1 = 0.3 * RMT_RESOLUTION_HZ / 1000000,  // T1L = 0.3us
        },
        .flags.msb_first = 1,  // MSB first: G7..G0 R7..R0 B7..B0
    };
    ESP_RETURN_ON_ERROR(rmt_new_bytes_encoder(&bytes_cfg, &s_bytes_encoder),
                        TAG, "create bytes encoder failed");

    // Copy encoder for reset pulse
    rmt_copy_encoder_config_t copy_cfg = {};
    ESP_RETURN_ON_ERROR(rmt_new_copy_encoder(&copy_cfg, &s_copy_encoder),
                        TAG, "create copy encoder failed");

    // Reset code: 50us low
    uint32_t reset_ticks = RMT_RESOLUTION_HZ / 1000000 * 50 / 2;
    s_reset_code = (rmt_symbol_word_t){
        .level0 = 0, .duration0 = reset_ticks,
        .level1 = 0, .duration1 = reset_ticks,
    };

    // Test pattern: flash red 3 times
    for (int i = 0; i < 3; i++) {
        set_all_pixels(255, 0, 0);
        led_matrix_flush();
        vTaskDelay(pdMS_TO_TICKS(150));
        clear_leds();
        led_matrix_flush();
        vTaskDelay(pdMS_TO_TICKS(80));
    }
    ESP_LOGI(TAG, "LED test pattern done");

    // Start animation task
    BaseType_t task_ok = xTaskCreate(led_animation_task, "led_anim", 4096, NULL, 5, &s_anim_task);
    ESP_RETURN_ON_FALSE(task_ok == pdPASS, ESP_ERR_NO_MEM, TAG, "create anim task failed");

    ESP_LOGI(TAG, "LED matrix initialized: %dx%d SK6812 on GPIO%d",
             MATRIX_SIZE, MATRIX_SIZE, BOARD_RGB_LED_PIN);
    return ESP_OK;
}

void led_matrix_set_state(led_state_t state)
{
    xSemaphoreTake(s_led_mutex, portMAX_DELAY);
    if (s_current_state != state) {
        ESP_LOGI(TAG, "LED state: %d -> %d", s_current_state, state);
        s_current_state = state;
    }
    xSemaphoreGive(s_led_mutex);
}

led_state_t led_matrix_get_state(void)
{
    led_state_t state;
    xSemaphoreTake(s_led_mutex, portMAX_DELAY);
    state = s_current_state;
    xSemaphoreGive(s_led_mutex);
    return state;
}

void led_matrix_set_brightness(uint8_t brightness)
{
    s_brightness = brightness;
}

void led_matrix_set_ota_progress(uint8_t percent)
{
    if (percent > 100) percent = 100;
    s_ota_percent = percent;
}

void led_matrix_deinit(void)
{
    if (s_anim_task) { vTaskDelete(s_anim_task); s_anim_task = NULL; }
    if (s_rmt_chan) { rmt_disable(s_rmt_chan); rmt_del_channel(s_rmt_chan); s_rmt_chan = NULL; }
    if (s_bytes_encoder) { rmt_del_encoder(s_bytes_encoder); s_bytes_encoder = NULL; }
    if (s_copy_encoder) { rmt_del_encoder(s_copy_encoder); s_copy_encoder = NULL; }
    if (s_led_mutex) { vSemaphoreDelete(s_led_mutex); s_led_mutex = NULL; }
}
