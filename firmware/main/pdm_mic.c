#include <string.h>
#include "esp_log.h"
#include "esp_check.h"
#include "driver/i2s_std.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "pdm_mic.h"
#include "board_config.h"
#include "es8311_codec.h"

static const char *TAG = "pdm_mic";

static bool s_capturing = false;
static uint32_t s_session_id = 0;
static pdm_mic_data_cb_t s_data_cb = NULL;

#define DMA_BUF_SIZE    2048

typedef struct { int16_t buf[DMA_BUF_SIZE / 2]; } rbuf_t;

esp_err_t pdm_mic_init(pdm_mic_data_cb_t data_cb)
{
    ESP_RETURN_ON_FALSE(data_cb, ESP_ERR_INVALID_ARG, TAG, "cb NULL");
    s_data_cb = data_cb;

    // Initialize ES8311 codec + I2S
    esp_err_t err = es8311_codec_init();
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "ES8311 init failed: %s", esp_err_to_name(err));
        return err;
    }

    ESP_LOGI(TAG, "ES8311 mic initialized: 16kHz 16-bit mono");
    return ESP_OK;
}

esp_err_t pdm_mic_start(uint32_t session_id)
{
    if (s_capturing) return ESP_OK;
    s_session_id = session_id;
    s_capturing = true;
    esp_err_t err = es8311_start_capture(session_id);
    if (err == ESP_OK)
        ESP_LOGI(TAG, "started session=%u", session_id);
    return err;
}

esp_err_t pdm_mic_stop(void)
{
    if (!s_capturing) return ESP_OK;
    s_capturing = false;
    es8311_stop_capture();
    ESP_LOGI(TAG, "stopped session=%u", s_session_id);
    return ESP_OK;
}

bool pdm_mic_is_capturing(void) { return s_capturing; }

esp_err_t pdm_mic_deinit(void)
{
    s_capturing = false;
    es8311_codec_deinit();
    s_data_cb = NULL;
    return ESP_OK;
}

// ----- Read task: read 32kHz PCM from ES8311, decimate 2:1 to 16kHz -----

void pdm_mic_read_task(void *arg)
{
    ESP_LOGI(TAG, "read task (ES8311 32kHz→16kHz)");
    rbuf_t *rb = malloc(sizeof(*rb));
    if (!rb) { vTaskDelete(NULL); return; }

    static int16_t pcm[960];     // 16kHz output buffer
    static int pcnt = 0;
    static int32_t acc = 0;      // decimation accumulator
    static int d_cnt = 0;

    while (1) {
        if (!s_capturing) {
            vTaskDelay(pdMS_TO_TICKS(10));
            continue;
        }

        i2s_chan_handle_t i2s = es8311_get_i2s_handle();
        if (!i2s) { vTaskDelay(pdMS_TO_TICKS(10)); continue; }

        size_t br = 0;
        esp_err_t err = i2s_channel_read(i2s, rb->buf, sizeof(rb->buf), &br, pdMS_TO_TICKS(100));
        if (err != ESP_OK) {
            if (err == ESP_ERR_TIMEOUT) {
                static int tc = 0;
                if (++tc % 10 == 1) ESP_LOGW(TAG, "timeout (%d)", tc);
            } else {
                ESP_LOGW(TAG, "err: %s", esp_err_to_name(err));
            }
            continue;
        }

        size_t ns = br / 2;
        for (size_t i = 0; i < ns; i++) {
            acc += (int32_t)rb->buf[i];
            d_cnt++;
            if (d_cnt >= 2) {
                int32_t out = acc >> 1;
                if (out > 32767) out = 32767;
                if (out < -32768) out = -32768;
                pcm[pcnt++] = (int16_t)out;
                acc = 0; d_cnt = 0;
                if (pcnt >= 960) {
                    if (s_data_cb) s_data_cb(pcm, 960, s_session_id);
                    pcnt = 0;
                }
            }
        }
    }
    free(rb);
    vTaskDelete(NULL);
}
