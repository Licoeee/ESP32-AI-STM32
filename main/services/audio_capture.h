#pragma once

#include <stddef.h>
#include <stdint.h>

#include "esp_err.h"

enum {
    AUDIO_CAPTURE_FRAME_SAMPLES = 320,
};

typedef struct {
    int16_t pcm[AUDIO_CAPTURE_FRAME_SAMPLES];
    size_t samples;
    uint32_t seq;
} audio_capture_frame_t;

esp_err_t audio_capture_init(void);
esp_err_t audio_capture_read(audio_capture_frame_t *frame, size_t *bytes_read);