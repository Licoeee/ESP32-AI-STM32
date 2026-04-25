#pragma once

#include <stddef.h>
#include <stdint.h>

#include "audio_capture.h"

void audio_transport_set_asr_ws_url(const char *ws_url);
const char *audio_transport_get_asr_ws_url(void);
void audio_transport_start(void);
void audio_transport_process_frame(const audio_capture_frame_t *frame);