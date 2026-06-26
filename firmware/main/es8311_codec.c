#include "es8311_codec.h"
#include "board_config.h"
#include <string.h>
#include "esp_log.h"
#include "esp_check.h"
#include "driver/i2c_master.h"
#include "driver/i2s_std.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "es8311";

static i2s_chan_handle_t s_rx_chan = NULL;
static bool s_capturing = false;
static uint32_t s_session_id = 0;
static i2c_master_bus_handle_t s_i2c_bus = NULL;
static i2c_master_dev_handle_t s_es8311_dev = NULL;
static i2c_master_dev_handle_t s_pca9557_dev = NULL;

static esp_err_t es8311_write(uint8_t reg, uint8_t val)
{
    uint8_t data[2] = {reg, val};
    return i2c_master_transmit(s_es8311_dev, data, sizeof(data), pdMS_TO_TICKS(100));
}

static esp_err_t pca9557_write(uint8_t reg, uint8_t val)
{
    uint8_t data[2] = {reg, val};
    return i2c_master_transmit(s_pca9557_dev, data, sizeof(data), pdMS_TO_TICKS(100));
}

esp_err_t es8311_codec_init(void)
{
    // 1. Init I2C master bus (external bus, GPIO38/39)
    i2c_master_bus_config_t bus_cfg = {
        .i2c_port = I2C_NUM_1,
        .sda_io_num = ES8311_I2C_SDA,
        .scl_io_num = ES8311_I2C_SCL,
        .clk_source = I2C_CLK_SRC_DEFAULT,
        .glitch_ignore_cnt = 7,
        .intr_priority = 0,
        .flags.enable_internal_pullup = true,
    };
    ESP_RETURN_ON_ERROR(i2c_new_master_bus(&bus_cfg, &s_i2c_bus), TAG, "new bus");

    // Probe
    esp_err_t probe;
    probe = i2c_master_probe(s_i2c_bus, ES8311_PCA9557_ADDR, 100);
    ESP_LOGI(TAG, "PCA9557@0x%02x: %s", ES8311_PCA9557_ADDR, probe == ESP_OK ? "FOUND" : "NOT FOUND");
    probe = i2c_master_probe(s_i2c_bus, ES8311_I2C_ADDR, 100);
    ESP_LOGI(TAG, "ES8311@0x%02x: %s", ES8311_I2C_ADDR, probe == ESP_OK ? "FOUND" : "NOT FOUND");

    // 2. Init PCA9557 (power on ES8311)
    i2c_device_config_t pca_cfg = {.device_address = ES8311_PCA9557_ADDR, .scl_speed_hz = 100000};
    if (i2c_master_bus_add_device(s_i2c_bus, &pca_cfg, &s_pca9557_dev) == ESP_OK) {
        pca9557_write(0x03, 0xF8);  // IO0-2 output
        pca9557_write(0x01, 0x07);  // power on
        vTaskDelay(pdMS_TO_TICKS(50));
        ESP_LOGI(TAG, "PCA9557: power on");
    }

    // 3. Init I2S at 32kHz → BCLK = 1.024MHz (ES8311 needs ≥1.024MHz for PLL)
    i2s_chan_config_t cc = I2S_CHANNEL_DEFAULT_CONFIG(I2S_NUM_0, I2S_ROLE_MASTER);
    ESP_RETURN_ON_ERROR(i2s_new_channel(&cc, NULL, &s_rx_chan), TAG, "I2S new");
    i2s_std_config_t sc = {
        .clk_cfg = I2S_STD_CLK_DEFAULT_CONFIG(32000),  // → BCLK = 1.024MHz
        .slot_cfg = I2S_STD_PHILIPS_SLOT_DEFAULT_CONFIG(I2S_DATA_BIT_WIDTH_16BIT, I2S_SLOT_MODE_MONO),
        .gpio_cfg = {
            .bclk = ES8311_I2S_BCK_PIN,
            .ws = ES8311_I2S_WS_PIN,
            .dout = I2S_GPIO_UNUSED,
            .din = ES8311_I2S_DIN_PIN,
        },
    };
    ESP_RETURN_ON_ERROR(i2s_channel_init_std_mode(s_rx_chan, &sc), TAG, "I2S std");
    ESP_LOGI(TAG, "I2S configured: BCLK=1.024MHz");

    // 4. Init ES8311 registers (from ESPHome coefficient for 1024000/16000)
    i2c_device_config_t es_cfg = {.device_address = ES8311_I2C_ADDR, .scl_speed_hz = 100000};
    ESP_RETURN_ON_ERROR(i2c_master_bus_add_device(s_i2c_bus, &es_cfg, &s_es8311_dev), TAG, "add ES8311");

    // Reset
    es8311_write(0x00, 0x1F);
    es8311_write(0x00, 0x00);

    // Clock: use BCLK as MCLK source (BIT7=1), enable all clocks
    es8311_write(0x01, 0xBF);  // SCLK as MCLK, all clocks on

    // Clock divider: pre_div=1+1=2, pre_mult=4 (mclk=1024000/2*4=2048000)
    es8311_write(0x02, (0x01-1)<<5 | 0x04<<3);  // 0x20: pre_div=1, pre_mult=4

    // ADC OSR: fs_mode=0, osr=16 (64×fs)
    es8311_write(0x03, 0x10);

    // DAC OSR: 32 (128×fs)
    es8311_write(0x04, 0x20);

    // ADC/DAC divider: adc_div=1+1=2, dac_div=1+1=2
    es8311_write(0x05, (1-1)<<4 | (1-1)<<0);  // 0x00: both =1

    // BCLK: div=4 (master mode BCLK out)
    es8311_write(0x06, 0x04);

    // LRCK dividers
    es8311_write(0x07, 0x00);  // lrck_h=0
    es8311_write(0x08, 0xFF);  // lrck_l=255

    // SDP in/out: 16-bit
    es8311_write(0x09, 3<<2);  // SDPIN: 16-bit
    es8311_write(0x0A, 3<<2);  // SDPOUT: 16-bit

    // Power up analog
    es8311_write(0x0D, 0x01);  // power up
    es8311_write(0x0E, 0x02);  // enable PGA + ADC modulator
    es8311_write(0x12, 0x00);  // enable DAC
    es8311_write(0x13, 0x10);  // output to HP drive

    // Microphone: analog PGA, max gain
    es8311_write(0x14, 0x1A);  // analog mic enable

    // ADC settings
    es8311_write(0x15, 0x00);  // ramp rate
    es8311_write(0x16, 0x00);  // ADC gain scale
    es8311_write(0x17, 0xC8);  // ADC volume (~6dB)

    // Bypass equalizers
    es8311_write(0x1C, 0x6A);  // ADC EQ bypass, HPF on
    es8311_write(0x37, 0x08);  // DAC EQ bypass

    // DAC volume: full
    es8311_write(0x32, 0xBF);  // 0dB

    // Power on
    es8311_write(0x00, 0x80);  // power on

    ESP_LOGI(TAG, "ES8311 init OK: 32kHz I2S → 16kHz PCM (SW decimate)");
    return ESP_OK;
}

esp_err_t es8311_start_capture(uint32_t session_id)
{
    if (s_capturing) return ESP_OK;
    s_session_id = session_id;
    s_capturing = true;
    return i2s_channel_enable(s_rx_chan);
}

esp_err_t es8311_stop_capture(void)
{
    if (!s_capturing) return ESP_OK;
    s_capturing = false;
    i2s_channel_disable(s_rx_chan);
    return ESP_OK;
}

bool es8311_is_capturing(void) { return s_capturing; }
i2s_chan_handle_t es8311_get_i2s_handle(void) { return s_rx_chan; }

esp_err_t es8311_codec_deinit(void)
{
    s_capturing = false;
    if (s_rx_chan) { i2s_del_channel(s_rx_chan); s_rx_chan = NULL; }
    if (s_i2c_bus) { i2c_del_master_bus(s_i2c_bus); s_i2c_bus = NULL; s_es8311_dev = NULL; s_pca9557_dev = NULL; }
    return ESP_OK;
}
