/*
 * 文件说明：
 * - 用途：负责应用层初始化编排，按顺序启动各业务模块。
 */

#include "app_init.h"

#include "app_config.h"
#include "asr_service.h"
#include "esp_log.h"
#include "wifi_service.h"

static const char *TAG = "app_init";

void app_init_start(void)
{
    ESP_LOGI(TAG, "应用初始化开始，准备启动 WiFi 和 ASR 音频链路");
    wifi_service_init_sta();
    asr_service_set_ws_url(APP_ASR_WS_URL);
    asr_service_start();
    ESP_LOGI(TAG, "应用初始化完成");
}
