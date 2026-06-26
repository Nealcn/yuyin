#pragma once

#include "esp_err.h"
#include "driver/i2s_std.h"

#ifdef __cplusplus
extern "C" {
#endif

esp_err_t es8311_codec_init(void);
esp_err_t es8311_codec_deinit(void);
esp_err_t es8311_start_capture(uint32_t session_id);
esp_err_t es8311_stop_capture(void);
bool es8311_is_capturing(void);
i2s_chan_handle_t es8311_get_i2s_handle(void);

#ifdef __cplusplus
}
#endif
