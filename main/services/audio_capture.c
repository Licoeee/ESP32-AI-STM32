/*
 * 文件说明：
 * - 用途：负责麦克风音频采集与 I2S 初始化。
 */

#include "audio_capture.h"

#include "app_config.h"
#include "board_pins.h"
#include "driver/i2s_std.h"
#include "esp_err.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#define AUDIO_CAPTURE_TAG "audio_capture"
#define AUDIO_CAPTURE_FRAME_SAMPLES 320

static i2s_chan_handle_t s_rx_chan;

esp_err_t audio_capture_init(void)
{
    i2s_chan_config_t chan_cfg = I2S_CHANNEL_DEFAULT_CONFIG(I2S_NUM_0, I2S_ROLE_MASTER);
    chan_cfg.dma_desc_num = 8;
    chan_cfg.dma_frame_num = AUDIO_CAPTURE_FRAME_SAMPLES;

    i2s_chan_handle_t tx_chan = NULL;
    esp_err_t ret = i2s_new_channel(&chan_cfg, &tx_chan, &s_rx_chan);
    if (ret != ESP_OK) {
        ESP_LOGE(AUDIO_CAPTURE_TAG, "i2s_new_channel failed: %s", esp_err_to_name(ret));
        return ret;
    }

    i2s_std_config_t std_cfg = {
        .clk_cfg = I2S_STD_CLK_DEFAULT_CONFIG(APP_MIC_SAMPLE_RATE_HZ),
        .slot_cfg = I2S_STD_MSB_SLOT_DEFAULT_CONFIG(I2S_DATA_BIT_WIDTH_16BIT, I2S_SLOT_MODE_MONO),
        .gpio_cfg = {
            .mclk = I2S_GPIO_UNUSED,
            .bclk = BOARD_MIC_I2S_BCLK_GPIO,
            .ws = BOARD_MIC_I2S_WS_GPIO,
            .dout = I2S_GPIO_UNUSED,
            .din = BOARD_MIC_I2S_DIN_GPIO,
            .invert_flags = {
                .mclk_inv = false,
                .bclk_inv = false,
                .ws_inv = false,
            },
        },
    };

    std_cfg.slot_cfg.slot_mask = I2S_STD_SLOT_LEFT;
    std_cfg.slot_cfg.ws_width = 16;
    std_cfg.slot_cfg.data_bit_width = I2S_DATA_BIT_WIDTH_16BIT;

    ret = i2s_channel_init_std_mode(s_rx_chan, &std_cfg);
    if (ret != ESP_OK) {
        ESP_LOGE(AUDIO_CAPTURE_TAG, "i2s_channel_init_std_mode failed: %s", esp_err_to_name(ret));
        return ret;
    }

    ret = i2s_channel_enable(s_rx_chan);
    if (ret != ESP_OK) {
        ESP_LOGE(AUDIO_CAPTURE_TAG, "i2s_channel_enable failed: %s", esp_err_to_name(ret));
        return ret;
    }

    ESP_LOGI(AUDIO_CAPTURE_TAG, "麦克风 I2S 初始化成功，BCLK=%d，WS=%d，DIN=%d", BOARD_MIC_I2S_BCLK_GPIO, BOARD_MIC_I2S_WS_GPIO, BOARD_MIC_I2S_DIN_GPIO);
    return ESP_OK;
}

esp_err_t audio_capture_read(audio_capture_frame_t *frame, size_t *bytes_read)
{
    if (frame == NULL || bytes_read == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    esp_err_t ret = i2s_channel_read(s_rx_chan, frame->pcm, sizeof(frame->pcm), bytes_read, pdMS_TO_TICKS(1000));
    if (ret != ESP_OK || *bytes_read == 0) {
        return ret;
    }

    size_t samples = *bytes_read / sizeof(int16_t);
    if (samples == 0) {
        return ESP_FAIL;
    }

    return ESP_OK;
}