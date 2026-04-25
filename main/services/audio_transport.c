/*
 * 文件说明：
 * - 用途：负责音频数据的 WebSocket 传输与 ASR 返回处理。
 */

#include "audio_transport.h"

#include <inttypes.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#include "app_config.h"
#include "audio_capture.h"
#include "esp_err.h"
#include "esp_log.h"
#include "esp_websocket_client.h"
#include "freertos/FreeRTOS.h"

#define AUDIO_TRANSPORT_TAG "audio_transport"
#define AUDIO_TRANSPORT_WS_URL_MAX_LEN 128
#define AUDIO_TRANSPORT_WS_SEND_TIMEOUT_MS 3000
#define AUDIO_TRANSPORT_BATCH_FRAMES 4

static esp_websocket_client_handle_t s_ws_client;
static bool s_ws_connected;
static bool s_transport_started;
static char s_asr_ws_url[AUDIO_TRANSPORT_WS_URL_MAX_LEN] = APP_ASR_WS_URL;

static void audio_transport_ws_send_text(const char *text)
{
    if (!s_ws_connected || s_ws_client == NULL || text == NULL) {
        return;
    }

    esp_websocket_client_send_text(s_ws_client, text, strlen(text), pdMS_TO_TICKS(AUDIO_TRANSPORT_WS_SEND_TIMEOUT_MS));
}

static void audio_transport_ws_send_pcm(const int16_t *pcm, size_t samples)
{
    if (!s_ws_connected || s_ws_client == NULL || pcm == NULL || samples == 0) {
        return;
    }

    const int bytes = (int)(samples * sizeof(int16_t));
    esp_websocket_client_send_bin(s_ws_client, (const char *)pcm, bytes, pdMS_TO_TICKS(AUDIO_TRANSPORT_WS_SEND_TIMEOUT_MS));
}

static void audio_transport_log_asr_payload(const char *payload, int len)
{
    if (payload == NULL || len <= 0) {
        return;
    }

    char buf[256];
    int copy_len = len;
    if (copy_len >= (int)sizeof(buf)) {
        copy_len = (int)sizeof(buf) - 1;
    }
    memcpy(buf, payload, copy_len);
    buf[copy_len] = '\0';

    const char *text_key = "\"text\":";
    char *text_pos = strstr(buf, text_key);
    if (text_pos != NULL) {
        text_pos += strlen(text_key);
        while (*text_pos == ' ' || *text_pos == '\t') {
            text_pos++;
        }
        if (*text_pos == '"') {
            text_pos++;
            char *end = strchr(text_pos, '"');
            if (end != NULL) {
                *end = '\0';
            }
        }
        ESP_LOGI(AUDIO_TRANSPORT_TAG, "ASR text: %s", text_pos);
        return;
    }

    ESP_LOGI(AUDIO_TRANSPORT_TAG, "ASR payload: %s", buf);
}

static void audio_transport_websocket_event_handler(void *handler_args, esp_event_base_t base, int32_t event_id, void *event_data)
{
    (void)handler_args;
    (void)base;
    esp_websocket_event_data_t *data = (esp_websocket_event_data_t *)event_data;

    switch (event_id) {
    case WEBSOCKET_EVENT_CONNECTED:
        s_ws_connected = true;
        ESP_LOGI(AUDIO_TRANSPORT_TAG, "websocket connected");
        audio_transport_ws_send_text("{\"type\":\"start\",\"sample_rate\":16000,\"bits\":16,\"channels\":1,\"format\":\"pcm_s16le\"}");
        break;
    case WEBSOCKET_EVENT_DISCONNECTED:
        s_ws_connected = false;
        ESP_LOGW(AUDIO_TRANSPORT_TAG, "websocket disconnected");
        break;
    case WEBSOCKET_EVENT_DATA:
        if (data != NULL && data->data_ptr != NULL && data->data_len > 0) {
            audio_transport_log_asr_payload(data->data_ptr, data->data_len);
        }
        break;
    case WEBSOCKET_EVENT_ERROR:
        ESP_LOGE(AUDIO_TRANSPORT_TAG, "websocket error");
        break;
    default:
        break;
    }
}

void audio_transport_set_asr_ws_url(const char *ws_url)
{
    if (ws_url == NULL || ws_url[0] == '\0') {
        return;
    }

    strlcpy(s_asr_ws_url, ws_url, sizeof(s_asr_ws_url));
    ESP_LOGI(AUDIO_TRANSPORT_TAG, "ASR websocket url set to %s", s_asr_ws_url);
}

const char *audio_transport_get_asr_ws_url(void)
{
    return s_asr_ws_url;
}

void audio_transport_start(void)
{
    if (s_transport_started) {
        return;
    }

    esp_websocket_client_config_t ws_cfg = {
        .uri = s_asr_ws_url,
        .disable_auto_reconnect = false,
        .reconnect_timeout_ms = 3000,
        .buffer_size = 2048,
        .network_timeout_ms = AUDIO_TRANSPORT_WS_SEND_TIMEOUT_MS,
    };

    s_ws_client = esp_websocket_client_init(&ws_cfg);
    if (s_ws_client == NULL) {
        ESP_LOGE(AUDIO_TRANSPORT_TAG, "websocket client init failed");
        return;
    }

    ESP_ERROR_CHECK(esp_websocket_register_events(s_ws_client, WEBSOCKET_EVENT_ANY, audio_transport_websocket_event_handler, NULL));
    ESP_ERROR_CHECK(esp_websocket_client_start(s_ws_client));

    s_transport_started = true;
}

void audio_transport_process_frame(const audio_capture_frame_t *frame)
{
    static int16_t batch_pcm[AUDIO_CAPTURE_FRAME_SAMPLES * AUDIO_TRANSPORT_BATCH_FRAMES];
    static size_t batch_samples = 0;
    static uint32_t batch_seq = 0;

    if (frame == NULL || frame->samples == 0) {
        return;
    }

    size_t copy_samples = frame->samples;
    if (copy_samples > AUDIO_CAPTURE_FRAME_SAMPLES) {
        copy_samples = AUDIO_CAPTURE_FRAME_SAMPLES;
    }

    if (batch_samples + copy_samples > (sizeof(batch_pcm) / sizeof(batch_pcm[0]))) {
        audio_transport_ws_send_pcm(batch_pcm, batch_samples);
        ESP_LOGI(AUDIO_TRANSPORT_TAG, "ws pcm flush seq=%" PRIu32 " samples=%u", batch_seq, (unsigned)batch_samples);
        batch_samples = 0;
    }

    memcpy(&batch_pcm[batch_samples], frame->pcm, copy_samples * sizeof(int16_t));
    batch_samples += copy_samples;
    batch_seq = frame->seq;

    if (batch_samples >= AUDIO_CAPTURE_FRAME_SAMPLES * AUDIO_TRANSPORT_BATCH_FRAMES) {
        audio_transport_ws_send_pcm(batch_pcm, batch_samples);
        ESP_LOGI(AUDIO_TRANSPORT_TAG, "ws pcm sent seq=%" PRIu32 " samples=%u", batch_seq, (unsigned)batch_samples);
        batch_samples = 0;
    }
}