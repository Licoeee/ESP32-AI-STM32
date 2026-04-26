/*
 * 文件说明：
 * - 用途：负责功放 I2S 输出与喇叭回放。
 */

#include "audio_playback.h"

#include <stddef.h>
#include <stdint.h>

#include "board_pins.h"
#include "driver/i2s_std.h"
#include "esp_err.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#define AUDIO_PLAYBACK_TAG "audio_playback"
#define AUDIO_PLAYBACK_SAMPLE_RATE_HZ 16000
#define AUDIO_PLAYBACK_BITS_PER_SAMPLE 16
#define AUDIO_PLAYBACK_FRAME_SAMPLES 320

static i2s_chan_handle_t s_tx_chan;

esp_err_t audio_playback_init(void)
{
    i2s_chan_config_t chan_cfg = I2S_CHANNEL_DEFAULT_CONFIG(I2S_NUM_1, I2S_ROLE_MASTER);
    chan_cfg.dma_desc_num = 8;
    chan_cfg.dma_frame_num = AUDIO_PLAYBACK_FRAME_SAMPLES;

    i2s_chan_handle_t rx_chan = NULL;
    esp_err_t ret = i2s_new_channel(&chan_cfg, &s_tx_chan, &rx_chan);
    if (ret != ESP_OK) {
        ESP_LOGE(AUDIO_PLAYBACK_TAG, "i2s_new_channel failed: %s", esp_err_to_name(ret));
        return ret;
    }

    i2s_std_config_t std_cfg = {
        .clk_cfg = I2S_STD_CLK_DEFAULT_CONFIG(AUDIO_PLAYBACK_SAMPLE_RATE_HZ),
        .slot_cfg = I2S_STD_MSB_SLOT_DEFAULT_CONFIG(I2S_DATA_BIT_WIDTH_16BIT, I2S_SLOT_MODE_MONO),
        .gpio_cfg = {
            .mclk = I2S_GPIO_UNUSED,
            .bclk = BOARD_SPK_I2S_BCLK_GPIO,
            .ws = BOARD_SPK_I2S_WS_GPIO,
            .dout = BOARD_SPK_I2S_DOUT_GPIO,
            .din = I2S_GPIO_UNUSED,
            .invert_flags = {
                .mclk_inv = false,
                .bclk_inv = false,
                .ws_inv = false,
            },
        },
    };

    std_cfg.slot_cfg.slot_mask = I2S_STD_SLOT_LEFT;
    std_cfg.slot_cfg.ws_width = AUDIO_PLAYBACK_BITS_PER_SAMPLE;
    std_cfg.slot_cfg.data_bit_width = I2S_DATA_BIT_WIDTH_16BIT;

    ret = i2s_channel_init_std_mode(s_tx_chan, &std_cfg);
    if (ret != ESP_OK) {
        ESP_LOGE(AUDIO_PLAYBACK_TAG, "i2s_channel_init_std_mode failed: %s", esp_err_to_name(ret));
        return ret;
    }

    ret = i2s_channel_enable(s_tx_chan);
    if (ret != ESP_OK) {
        ESP_LOGE(AUDIO_PLAYBACK_TAG, "i2s_channel_enable failed: %s", esp_err_to_name(ret));
        return ret;
    }

    ESP_LOGI(AUDIO_PLAYBACK_TAG, "功放 I2S 初始化成功，BCLK=%d，WS=%d，DOUT=%d", BOARD_SPK_I2S_BCLK_GPIO, BOARD_SPK_I2S_WS_GPIO, BOARD_SPK_I2S_DOUT_GPIO);
    return ESP_OK;
}

esp_err_t audio_playback_write(const int16_t *pcm, size_t samples)
{
    if (s_tx_chan == NULL || pcm == NULL || samples == 0) {
        return ESP_ERR_INVALID_ARG;
    }

    const size_t copy_samples = samples > AUDIO_PLAYBACK_FRAME_SAMPLES ? AUDIO_PLAYBACK_FRAME_SAMPLES : samples;
    size_t bytes_written = 0;
    esp_err_t ret = i2s_channel_write(s_tx_chan, pcm, copy_samples * sizeof(int16_t), &bytes_written, portMAX_DELAY);
    if (ret != ESP_OK) {
        return ret;
    }

    return (bytes_written == copy_samples * sizeof(int16_t)) ? ESP_OK : ESP_FAIL;
}