#pragma once

#include <stdint.h>
#include <stdbool.h>
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Callback for captured PCM audio data
 * @param data  Pointer to 16-bit mono PCM data
 * @param samples Number of samples in this buffer
 * @param session_id Current recording session ID
 */
typedef void (*pdm_mic_data_cb_t)(const int16_t *data, size_t samples, uint32_t session_id);

/**
 * @brief Initialize the PDM microphone
 * @param data_cb Callback invoked when PCM data is ready
 * @return ESP_OK on success
 */
esp_err_t pdm_mic_init(pdm_mic_data_cb_t data_cb);

/**
 * @brief Start PDM microphone capture
 * @param session_id Recording session identifier
 * @return ESP_OK on success
 */
esp_err_t pdm_mic_start(uint32_t session_id);

/**
 * @brief Stop PDM microphone capture
 * @return ESP_OK on success
 */
esp_err_t pdm_mic_stop(void);

/**
 * @brief Check if the microphone is currently capturing
 */
bool pdm_mic_is_capturing(void);

/**
 * @brief Deinitialize and free resources
 */
esp_err_t pdm_mic_deinit(void);

#ifdef __cplusplus
}
#endif
