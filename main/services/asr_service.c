/*
 * 文件说明：
 * - 用途：封装 ASR 服务入口与 WebSocket 地址配置。
 */

#include "asr_service.h"

#include "audio_service.h"
#include "app_config.h"

void asr_service_start(void)
{
    audio_service_set_asr_ws_url(APP_ASR_WS_URL);
    audio_service_start();
}

void asr_service_set_ws_url(const char *ws_url)
{
    audio_service_set_asr_ws_url(ws_url);
}

const char *asr_service_get_ws_url(void)
{
    return audio_service_get_asr_ws_url();
}