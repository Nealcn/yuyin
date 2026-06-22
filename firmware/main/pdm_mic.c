#include <string.h>
#include "esp_log.h"
#include "esp_check.h"
#include "driver/i2s_std.h"
#include "driver/i2s_pdm.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "pdm_mic.h"
#include "board_config.h"

static const char *TAG = "pdm_mic";

// I2S PDM RX channel handle
static i2s_chan_handle_t s_rx_chan = NULL;
static bool s_capturing = false;
static uint32_t s_session_id = 0;
static pdm_mic_data_cb_t s_data_cb = NULL;

// DMA buffer: 20 ms of audio at 16 kHz / 16-bit mono = 320 bytes
#define DMA_BUFFER_SIZE    (BOARD_AUDIO_SAMPLE_RATE * 2 * BOARD_AUDIO_CHANNELS / 50) // 20ms
#define DMA_BUFFER_COUNT   4

esp_err_t pdm_mic_init(pdm_mic_data_cb_t data_cb)
{
    ESP_RETURN_ON_FALSE(data_cb != NULL, ESP_ERR_INVALID_ARG, TAG, "data callback must not be NULL");

    s_data_cb = data_cb;

    if (s_rx_chan != NULL) {
        ESP_LOGW(TAG, "I2S already initialized, re-initializing");
        pdm_mic_deinit();
    }

    // ---- Configure I2S PDM RX channel ----
    i2s_chan_config_t chan_cfg = I2S_CHANNEL_DEFAULT_CONFIG(I2S_NUM_AUTO, I2S_ROLE_MASTER);
    chan_cfg.auto_clear = true; // auto-clear old data before new DMA starts
    ESP_RETURN_ON_ERROR(
        i2s_new_channel(&chan_cfg, NULL, &s_rx_chan),
        TAG, "create I2S RX channel failed"
    );

    // PDM RX mode config
    i2s_pdm_rx_config_t pdm_rx_cfg = {
        .clk_cfg = {
            .sample_rate_hz = BOARD_AUDIO_SAMPLE_RATE,
            .clk_src = I2S_CLK_SRC_DEFAULT,
        },
        .slot_cfg = {
            .slot_mode = I2S_SLOT_MODE_MONO,
            .data_bit_width = I2S_DATA_BIT_WIDTH_16BIT,
            .slot_bit_width = I2S_SLOT_BIT_WIDTH_AUTO,
        },
        .gpio_cfg = {
            .clk = BOARD_PDM_CLK_PIN,
            .din = BOARD_PDM_DATA_PIN,
            .invert_flags = {
                .clk_inv = false,
            },
        },
    };

    ESP_RETURN_ON_ERROR(
        i2s_channel_init_pdm_rx_mode(s_rx_chan, &pdm_rx_cfg),
        TAG, "initialize I2S PDM RX channel failed"
    );

    ESP_LOGI(TAG, "PDM mic initialized: %d Hz, 16-bit, mono, PDM_CLK=%d, PDM_DATA=%d",
             BOARD_AUDIO_SAMPLE_RATE, BOARD_PDM_CLK_PIN, BOARD_PDM_DATA_PIN);

    return ESP_OK;
}

esp_err_t pdm_mic_start(uint32_t session_id)
{
    ESP_RETURN_ON_FALSE(s_rx_chan != NULL, ESP_ERR_INVALID_STATE, TAG, "I2S not initialized");

    if (s_capturing) {
        ESP_LOGW(TAG, "already capturing");
        return ESP_OK;
    }

    s_session_id = session_id;
    s_capturing = true;

    // Enable the I2S RX channel
    ESP_RETURN_ON_ERROR(
        i2s_channel_enable(s_rx_chan),
        TAG, "enable I2S RX channel failed"
    );

    ESP_LOGI(TAG, "PDM capture started, session=%u", session_id);
    return ESP_OK;
}

esp_err_t pdm_mic_stop(void)
{
    if (!s_capturing || s_rx_chan == NULL) {
        return ESP_OK;
    }

    s_capturing = false;

    esp_err_t err = i2s_channel_disable(s_rx_chan);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "disable I2S channel failed: %s", esp_err_to_name(err));
    }

    ESP_LOGI(TAG, "PDM capture stopped, session=%u", s_session_id);
    return err;
}

bool pdm_mic_is_capturing(void)
{
    return s_capturing;
}

esp_err_t pdm_mic_deinit(void)
{
    if (s_capturing) {
        pdm_mic_stop();
    }

    if (s_rx_chan != NULL) {
        esp_err_t err = i2s_del_channel(s_rx_chan);
        s_rx_chan = NULL;
        if (err != ESP_OK) {
            ESP_LOGW(TAG, "delete I2S channel failed: %s", esp_err_to_name(err));
            return err;
        }
    }

    s_data_cb = NULL;
    return ESP_OK;
}

// ----- Task to read from I2S and invoke callback -----
// This runs as a dedicated task because i2s_channel_read is a blocking call.
// We read small DMA chunks and forward them to the callback (Opus encoder).

typedef struct {
    int16_t buffer[DMA_BUFFER_SIZE / 2]; // 16-bit samples, half size
} pdm_read_buf_t;

void pdm_mic_read_task(void *arg)
{
    ESP_LOGI(TAG, "PDM read task started");

    pdm_read_buf_t *buf = malloc(sizeof(pdm_read_buf_t));
    if (!buf) {
        ESP_LOGE(TAG, "failed to allocate read buffer");
        vTaskDelete(NULL);
        return;
    }

    while (1) {
        // Wait until capturing starts
        if (!s_capturing) {
            vTaskDelay(pdMS_TO_TICKS(10));
            continue;
        }

        size_t bytes_read = 0;
        esp_err_t err = i2s_channel_read(s_rx_chan, buf->buffer, sizeof(buf->buffer), &bytes_read, portMAX_DELAY);
        if (err != ESP_OK) {
            ESP_LOGW(TAG, "I2S read error: %s", esp_err_to_name(err));
            continue;
        }

        if (bytes_read > 0 && s_data_cb) {
            size_t samples = bytes_read / sizeof(int16_t);
            s_data_cb(buf->buffer, samples, s_session_id);
        }
    }

    free(buf);
    vTaskDelete(NULL);
}
