/**
 * @file main.c
 * @brief M5AtomS3 Voice Stick - Main application state machine
 *
 * States:
 *   ADVERTISING → IDLE → RECORDING → WAITING_ASR → COMPLETE → IDLE
 *                                    ↑                │
 *                                    └── Cancel ──────┘
 *
 * Architecture:
 *   PDM Mic → Opus encode → BLE notify → Windows Desktop → ASR → Paste
 *
 * Single button:
 *   Press down  → start recording, LED red
 *   Release     → stop, send end-of-session, LED yellow (waiting ASR)
 *   Double-click → cancel recording or discard ASR result, LED red X
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "esp_log.h"
#include "esp_check.h"
#include "esp_system.h"
#include "esp_timer.h"
#include "nvs_flash.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "board_config.h"
#include "pdm_mic.h"
#include "opus_encoder.h"
#include "ble_service.h"
#include "led_matrix.h"
#include "button.h"

static const char *TAG = "atom_voicestick";

// ---- Application states ----
typedef enum {
    APP_STATE_ADVERTISING,       // BLE advertising, not connected
    APP_STATE_IDLE,              // Connected, ready to record
    APP_STATE_RECORDING,         // Capturing and transmitting audio
    APP_STATE_WAITING_ASR,       // Audio sent, waiting for ASR result
    APP_STATE_COMPLETE,          // Received result → will return to idle
} app_state_t;

static app_state_t s_state = APP_STATE_ADVERTISING;
static uint32_t s_session_id = 0;
static uint32_t s_seq = 0;
static bool s_asr_success = false;

// PCM accumulation buffer (Opus frame buffer)
static int16_t *s_pcm_buffer = NULL;
static size_t s_pcm_count = 0;
static size_t s_pcm_capacity = 0;

// ---- State name helper ----
static const char *state_name(app_state_t state)
{
    switch (state) {
    case APP_STATE_ADVERTISING:  return "ADVERTISING";
    case APP_STATE_IDLE:         return "IDLE";
    case APP_STATE_RECORDING:    return "RECORDING";
    case APP_STATE_WAITING_ASR:  return "WAITING_ASR";
    case APP_STATE_COMPLETE:     return "COMPLETE";
    default:                     return "UNKNOWN";
    }
}

static void set_state(app_state_t new_state)
{
    if (s_state != new_state) {
        ESP_LOGI(TAG, "state: %s → %s", state_name(s_state), state_name(new_state));
        s_state = new_state;
    }
}

// ---- PCM accumulation (callback from PDM mic) ----
static void on_pcm_data(const int16_t *data, size_t samples, uint32_t session_id)
{
    if (s_state != APP_STATE_RECORDING) {
        return;
    }

    // Accumulate PCM samples
    size_t space = s_pcm_capacity - s_pcm_count;
    if (samples > space) {
        ESP_LOGW(TAG, "PCM buffer overflow, dropping %u samples", (unsigned)(samples - space));
        samples = space;
    }

    memcpy(s_pcm_buffer + s_pcm_count, data, samples * sizeof(int16_t));
    s_pcm_count += samples;

    // When we have a full Opus frame, encode and send
    size_t frame_samples = audio_encoder_frame_samples();
    while (s_pcm_count >= frame_samples) {
        // Encode to Opus
        uint8_t opus_data[400]; // Max Opus frame size
        size_t opus_len = 0;

        esp_err_t err = audio_encode_frame(s_pcm_buffer, frame_samples,
                                          opus_data, sizeof(opus_data), &opus_len);
        if (err != ESP_OK) {
            ESP_LOGW(TAG, "Opus encode error: %s", esp_err_to_name(err));
            // Skip this frame
        } else if (opus_len > 0) {
            // Send via BLE
            uint8_t flags = 0;
            if (s_seq == 0) {
                flags |= AUDIO_FRAME_FLAG_START;
            }
            ble_service_send_audio(opus_data, opus_len, session_id, s_seq, flags);
            s_seq++;
        }

        // Shift remaining data
        size_t remaining = s_pcm_count - frame_samples;
        if (remaining > 0) {
            memmove(s_pcm_buffer, s_pcm_buffer + frame_samples, remaining * sizeof(int16_t));
        }
        s_pcm_count = remaining;
    }
}

// ---- Send end-of-stream frame ----
static void send_audio_end(uint32_t session_id)
{
    if (ble_service_is_connected()) {
        ble_service_send_audio(NULL, 0, session_id, s_seq, AUDIO_FRAME_FLAG_END);
    }
}

// ---- Send button state over BLE ----
static void send_button_down(const char *button, uint32_t session_id)
{
    char json[128];
    snprintf(json, sizeof(json),
             "{\"event\":\"button_down\",\"button\":\"%s\",\"session_id\":%lu}",
             button, (unsigned long)session_id);
    ble_service_send_state(json);
}

static void send_button_up(const char *button, uint32_t duration_ms, uint32_t session_id)
{
    char json[128];
    snprintf(json, sizeof(json),
             "{\"event\":\"button_up\",\"button\":\"%s\",\"duration_ms\":%lu,\"session_id\":%lu}",
             button, (unsigned long)duration_ms, (unsigned long)session_id);
    ble_service_send_state(json);
}

static void send_button_click(const char *button, uint32_t duration_ms, uint32_t session_id)
{
    char json[128];
    snprintf(json, sizeof(json),
             "{\"event\":\"button_down\",\"button\":\"%s\",\"session_id\":%lu}",
             button, (unsigned long)session_id);
    ble_service_send_state(json);
    snprintf(json, sizeof(json),
             "{\"event\":\"button_up\",\"button\":\"%s\",\"duration_ms\":%lu,\"session_id\":%lu}",
             button, (unsigned long)duration_ms, (unsigned long)session_id);
    ble_service_send_state(json);
}

// ---- Start recording session ----
static void start_recording(void)
{
    s_session_id++;
    s_seq = 0;
    s_pcm_count = 0;
    s_asr_success = false;

    // Allocate PCM buffer (enough for 2 seconds worst-case accumulation)
    size_t max_samples = BOARD_AUDIO_SAMPLE_RATE * 2; // 2 seconds @ 16kHz
    if (!s_pcm_buffer) {
        s_pcm_buffer = malloc(max_samples * sizeof(int16_t));
        if (!s_pcm_buffer) {
            ESP_LOGE(TAG, "failed to allocate PCM buffer");
            led_matrix_set_state(LED_STATE_ERROR);
            return;
        }
        s_pcm_capacity = max_samples;
    }

    esp_err_t err = pdm_mic_start(s_session_id);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "PDM start failed: %s", esp_err_to_name(err));
        led_matrix_set_state(LED_STATE_ERROR);
        return;
    }

    // Reset Opus encoder for new session
    audio_encoder_reset();

    set_state(APP_STATE_RECORDING);
    led_matrix_set_state(LED_STATE_RECORDING);

    send_button_down("primary", s_session_id);

    ESP_LOGI(TAG, "recording started: session=%u", s_session_id);
}

// ---- Stop recording and send audio ----
static void stop_recording(void)
{
    if (s_state != APP_STATE_RECORDING) {
        return;
    }

    uint32_t duration_ms = button_get_press_duration_ms();
    uint32_t session = s_session_id;

    // Stop PDM capture
    pdm_mic_stop();

    // Encode any remaining PCM in the buffer
    if (s_pcm_count > 0 && s_pcm_buffer) {
        size_t frame_samples = audio_encoder_frame_samples();
        // Pad with silence if needed
        while (s_pcm_count < frame_samples && s_pcm_count < s_pcm_capacity) {
            s_pcm_buffer[s_pcm_count] = 0;
            s_pcm_count++;
        }
        if (s_pcm_count >= frame_samples) {
            uint8_t opus_data[400];
            size_t opus_len = 0;
            if (audio_encode_frame(s_pcm_buffer, frame_samples,
                                  opus_data, sizeof(opus_data), &opus_len) == ESP_OK
                && opus_len > 0) {
                ble_service_send_audio(opus_data, opus_len, session, s_seq, 0);
                s_seq++;
            }
        }
    }

    // Send end-of-session marker
    send_audio_end(session);

    // Send button up event
    send_button_up("primary", duration_ms, session);

    // Only discard if recording was very short (< 0.5s, voicestick convention)
    if (duration_ms < 500) {
        ESP_LOGI(TAG, "recording too short (%u ms), discarding", duration_ms);
        set_state(APP_STATE_IDLE);
        led_matrix_set_state(LED_STATE_IDLE);
        s_pcm_count = 0;
        return;
    }

    // Wait for ASR result from Windows side
    set_state(APP_STATE_WAITING_ASR);
    led_matrix_set_state(LED_STATE_THINKING);

    ESP_LOGI(TAG, "recording stopped: session=%u, duration=%u ms", session, duration_ms);
}

// ---- Cancel current recording / waiting state ----
static void cancel_action(void)
{
    if (s_state == APP_STATE_RECORDING) {
        pdm_mic_stop();
        s_pcm_count = 0;
        ESP_LOGI(TAG, "recording cancelled");
    } else if (s_state == APP_STATE_WAITING_ASR) {
        ESP_LOGI(TAG, "ASR waiting cancelled");
    }

    // Send cancel event (as secondary button click in voicestick protocol)
    send_button_click("secondary", 0, 0);

    set_state(APP_STATE_IDLE);
    led_matrix_set_state(LED_STATE_CANCELLED);
}

// ---- Button callback ----
static void on_button_event(button_event_t event, uint32_t duration_ms)
{
    (void)duration_ms;

    switch (event) {
    case BUTTON_EVENT_PRESS_DOWN:
        if (s_state == APP_STATE_IDLE) {
            start_recording();
        }
        // In other states, press is ignored (handled on release or double-click)
        break;

    case BUTTON_EVENT_PRESS_UP:
        // This is only fired for single press (double-click timer expired)
        if (s_state == APP_STATE_RECORDING) {
            stop_recording();
        }
        break;

    case BUTTON_EVENT_DOUBLE_CLICK:
        // Cancel whatever we're doing
        if (s_state == APP_STATE_RECORDING || s_state == APP_STATE_WAITING_ASR) {
            cancel_action();
        }
        break;

    case BUTTON_EVENT_LONG_PRESS:
        // Reserved for future use (e.g., mode toggle, deep sleep)
        ESP_LOGI(TAG, "long press: %u ms (reserved)", duration_ms);
        break;
    }
}

// ---- BLE control callback ----
static void on_ble_control(const char *json)
{
    ESP_LOGI(TAG, "control: %s", json);

    // Parse JSON for ui_state updates from Windows app
    // Format: {"event":"ui_state","state":"ready","text":"..."}
    // We use this to know when ASR completes
    if (strstr(json, "ui_state")) {
        if (strstr(json, "\"state\":\"ready\"")) {
            if (s_state == APP_STATE_WAITING_ASR || s_state == APP_STATE_COMPLETE) {
                set_state(APP_STATE_IDLE);
                led_matrix_set_state(LED_STATE_IDLE);
            }
        } else if (strstr(json, "\"state\":\"pending_confirmation\"")) {
            // ASR result received and ready to paste
            if (s_state == APP_STATE_WAITING_ASR) {
                set_state(APP_STATE_COMPLETE);
                led_matrix_set_state(LED_STATE_CONFIRMED);
            }
        } else if (strstr(json, "\"state\":\"error\"")) {
            led_matrix_set_state(LED_STATE_ERROR);
            // Return to idle after a moment
            set_state(APP_STATE_IDLE);
        }
    } else if (strstr(json, "\"event\":\"interaction_mode\"")) {
        // Could handle interaction mode switching in the future
        ESP_LOGI(TAG, "interaction mode update ignored (hold-to-talk only)");
    }
}

// ---- BLE connection callback (from ble_service via state events) ----
// We handle connection state through the app lifecycle.
// The ble_service notifies us via control events and we track is_connected().

// ---- Entry point ----
void app_main(void)
{
    ESP_LOGI(TAG, "===== M5AtomS3 Voice Stick booting =====");

    // Initialize NVS (needed for BLE)
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    // Initialize BLE service FIRST (before other drivers to avoid heap conflicts)
    esp_err_t ble_err = ble_service_init(BOARD_BLE_DEVICE_PREFIX);
    if (ble_err != ESP_OK) {
        ESP_LOGE(TAG, "BLE init failed: %s, continuing without BLE", esp_err_to_name(ble_err));
    } else {
        ble_service_set_control_callback(on_ble_control);
    }

    // Initialize LED matrix (visual feedback)
    ESP_ERROR_CHECK(led_matrix_init());
    led_matrix_set_state(LED_STATE_IDLE);

    // Initialize Opus encoder
    ESP_LOGI(TAG, "Initializing Opus encoder...");
    esp_err_t opus_err = audio_encoder_init(
        BOARD_AUDIO_SAMPLE_RATE,
        BOARD_AUDIO_CHANNELS,
        BOARD_AUDIO_FRAME_MS
    );
    if (opus_err != ESP_OK) {
        ESP_LOGW(TAG, "Opus init failed: %s, continuing without audio encoding", esp_err_to_name(opus_err));
    }

    // Initialize PDM microphone
    esp_err_t pdm_err = pdm_mic_init(on_pcm_data);
    if (pdm_err != ESP_OK) {
        ESP_LOGE(TAG, "PDM mic init failed: %s", esp_err_to_name(pdm_err));
    }
    // Note: pdm_mic is started/stopped per recording session

    // Initialize button
    ESP_ERROR_CHECK(button_init(on_button_event));

    // Create PDM read task (blocking I2S reads need dedicated task)
    // We start it in pdm_mic.c's design - the read loop is inside pdm_mic.c
    // which gets called when pdm_mic_start() is invoked.
    // For I2S PDM, we spawn the read task here.
    extern void pdm_mic_read_task(void *arg);
    BaseType_t task_ok = xTaskCreate(pdm_mic_read_task, "pdm_read", 4096, NULL, 6, NULL);
    if (task_ok != pdPASS) {
        ESP_LOGE(TAG, "failed to create PDM read task");
    }

    set_state(APP_STATE_ADVERTISING);
    led_matrix_set_state(LED_STATE_IDLE);

    ESP_LOGI(TAG, "===== M5AtomS3 Voice Stick ready =====");
    ESP_LOGI(TAG, "Device: %s", ble_service_device_name());

    // Enter advertising state - wait for BLE connection
    // The main work is done in tasks/ISRs; app_main just sets up and returns.
    // State transitions happen in button and BLE callbacks.
}
