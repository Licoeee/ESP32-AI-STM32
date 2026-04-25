/*
 * 文件说明：
 * - 用途：作为音频功能门面，串联采集与传输流程。
 */

#include "audio_service.h"

#include "audio_capture.h"
#include "audio_transport.h"
#include "esp_err.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#define AUDIO_SERVICE_TAG "audio_service"
#define AUDIO_SERVICE_TASK_STACK 6144
#define AUDIO_SERVICE_TASK_PRIORITY 8

static void audio_service_task(void *arg)
{
    (void)arg;
    audio_capture_frame_t frame = {0};
    size_t bytes_read = 0;

    while (true) {
        esp_err_t ret = audio_capture_read(&frame, &bytes_read);
        if (ret != ESP_OK || bytes_read == 0) {
            ESP_LOGW(AUDIO_SERVICE_TAG, "audio capture read failed: %s", esp_err_to_name(ret));
            vTaskDelay(pdMS_TO_TICKS(50));
            continue;
        }

        frame.samples = bytes_read / sizeof(int16_t);
        frame.seq++;
        audio_transport_process_frame(&frame);
    }
}

const char *audio_service_get_asr_ws_url(void)
{
    return audio_transport_get_asr_ws_url();
}

void audio_service_set_asr_ws_url(const char *ws_url)
{
    audio_transport_set_asr_ws_url(ws_url);
}

void audio_service_start(void)
{
    if (audio_capture_init() != ESP_OK) {
        ESP_LOGE(AUDIO_SERVICE_TAG, "audio capture init failed");
        return;
    }

    audio_transport_start();
    xTaskCreatePinnedToCore(audio_service_task, "audio_service", AUDIO_SERVICE_TASK_STACK, NULL, AUDIO_SERVICE_TASK_PRIORITY, NULL, 1);
    ESP_LOGI(AUDIO_SERVICE_TAG, "audio service started");
}