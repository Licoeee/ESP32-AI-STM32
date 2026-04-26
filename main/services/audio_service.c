/*
 * 文件说明：
 * - 用途：作为音频功能门面，串联采集、回放与传输流程。
 */

#include "audio_service.h"

#include "audio_capture.h"
#include "audio_playback.h"
#include "audio_transport.h"
#include "esp_err.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#define AUDIO_SERVICE_TAG "audio_service"
#define AUDIO_SERVICE_TASK_STACK 6144
#define AUDIO_SERVICE_TASK_PRIORITY 8
#define AUDIO_SERVICE_LOG_INTERVAL 50

static void audio_service_task(void *arg)
{
    (void)arg;
    audio_capture_frame_t frame = {0};
    size_t bytes_read = 0;
    uint32_t log_count = 0;

    while (true) {
        esp_err_t ret = audio_capture_read(&frame, &bytes_read);
        if (ret != ESP_OK || bytes_read == 0) {
            ESP_LOGW(AUDIO_SERVICE_TAG, "音频采集失败，错误=%s", esp_err_to_name(ret));
            vTaskDelay(pdMS_TO_TICKS(50));
            continue;
        }

        frame.samples = bytes_read / sizeof(int16_t);
        frame.seq++;

        if ((log_count++ % AUDIO_SERVICE_LOG_INTERVAL) == 0) {
            ESP_LOGI(AUDIO_SERVICE_TAG, "采集到音频帧，序号=%" PRIu32 "，样本数=%u", frame.seq, (unsigned)frame.samples);
        }

        audio_transport_process_frame(&frame);
        ret = audio_playback_write(frame.pcm, frame.samples);
        if (ret != ESP_OK) {
            ESP_LOGW(AUDIO_SERVICE_TAG, "音频回放失败，错误=%s", esp_err_to_name(ret));
        }
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
        ESP_LOGE(AUDIO_SERVICE_TAG, "音频采集模块初始化失败");
        return;
    }

    if (audio_playback_init() != ESP_OK) {
        ESP_LOGE(AUDIO_SERVICE_TAG, "音频回放模块初始化失败");
        return;
    }

    audio_transport_start();
    xTaskCreatePinnedToCore(audio_service_task, "audio_service", AUDIO_SERVICE_TASK_STACK, NULL, AUDIO_SERVICE_TASK_PRIORITY, NULL, 1);
    ESP_LOGI(AUDIO_SERVICE_TAG, "音频服务已启动，采集、回放、ASR 分流同时工作");
}