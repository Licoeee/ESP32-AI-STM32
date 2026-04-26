/*
 * 文件说明：
 * - 用途：负责功放 I2S 输出与喇叭回放。
 */

#pragma once

#include <stddef.h>
#include <stdint.h>

#include "esp_err.h"

esp_err_t audio_playback_init(void);
esp_err_t audio_playback_write(const int16_t *pcm, size_t samples);