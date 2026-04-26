/*
 * 文件说明：
 * - 用途：程序启动入口，负责完成基础初始化并进入应用流程。
 */

#include "app_init.h"
#include "esp_err.h"
#include "esp_log.h"
#include "nvs_flash.h"

void app_main(void)
{
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    app_init_start();
}
