/* WiFi 站点模式示例

   本示例代码属于公共领域（或根据您的选择采用 CC0 许可）。

   除非适用法律要求或书面同意，否则本软件按"原样"分发，
   不提供任何明示或暗示的保证或条件。
*/
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "esp_system.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_log.h"
#include "nvs_flash.h"

#include "lwip/err.h"
#include "lwip/sys.h"

/* 示例使用 WiFi 配置，您可以通过项目配置菜单进行设置

   如果您不想这样做，只需将下面的条目更改为您想要的配置字符串即可
   即 #define EXAMPLE_WIFI_SSID "mywifissid"
*/
#define EXAMPLE_ESP_WIFI_SSID      CONFIG_ESP_WIFI_SSID      // WiFi 名称
#define EXAMPLE_ESP_WIFI_PASS      CONFIG_ESP_WIFI_PASSWORD  // WiFi 密码
#define EXAMPLE_ESP_MAXIMUM_RETRY  CONFIG_ESP_MAXIMUM_RETRY  // 最大重试次数

#if CONFIG_ESP_STATION_EXAMPLE_WPA3_SAE_PWE_HUNT_AND_PECK
#define ESP_WIFI_SAE_MODE WPA3_SAE_PWE_HUNT_AND_PECK
#define EXAMPLE_H2E_IDENTIFIER ""
#elif CONFIG_ESP_STATION_EXAMPLE_WPA3_SAE_PWE_HASH_TO_ELEMENT
#define ESP_WIFI_SAE_MODE WPA3_SAE_PWE_HASH_TO_ELEMENT
#define EXAMPLE_H2E_IDENTIFIER CONFIG_ESP_WIFI_PW_ID
#elif CONFIG_ESP_STATION_EXAMPLE_WPA3_SAE_PWE_BOTH
#define ESP_WIFI_SAE_MODE WPA3_SAE_PWE_BOTH
#define EXAMPLE_H2E_IDENTIFIER CONFIG_ESP_WIFI_PW_ID
#endif

#if CONFIG_ESP_WIFI_AUTH_OPEN
#define ESP_WIFI_SCAN_AUTH_MODE_THRESHOLD WIFI_AUTH_OPEN
#elif CONFIG_ESP_WIFI_AUTH_WEP
#define ESP_WIFI_SCAN_AUTH_MODE_THRESHOLD WIFI_AUTH_WEP
#elif CONFIG_ESP_WIFI_AUTH_WPA_PSK
#define ESP_WIFI_SCAN_AUTH_MODE_THRESHOLD WIFI_AUTH_WPA_PSK
#elif CONFIG_ESP_WIFI_AUTH_WPA2_PSK
#define ESP_WIFI_SCAN_AUTH_MODE_THRESHOLD WIFI_AUTH_WPA2_PSK
#elif CONFIG_ESP_WIFI_AUTH_WPA_WPA2_PSK
#define ESP_WIFI_SCAN_AUTH_MODE_THRESHOLD WIFI_AUTH_WPA_WPA2_PSK
#elif CONFIG_ESP_WIFI_AUTH_WPA3_PSK
#define ESP_WIFI_SCAN_AUTH_MODE_THRESHOLD WIFI_AUTH_WPA3_PSK
#elif CONFIG_ESP_WIFI_AUTH_WPA2_WPA3_PSK
#define ESP_WIFI_SCAN_AUTH_MODE_THRESHOLD WIFI_AUTH_WPA2_WPA3_PSK
#elif CONFIG_ESP_WIFI_AUTH_WAPI_PSK
#define ESP_WIFI_SCAN_AUTH_MODE_THRESHOLD WIFI_AUTH_WAPI_PSK
#endif

/* FreeRTOS 事件组，用于标记连接状态 */
static EventGroupHandle_t s_wifi_event_group;

/* 事件组允许为每个事件设置多个位，但我们只关心两个事件：
 * - 已连接到 AP 并获得 IP 地址
 * - 达到最大重试次数后连接失败 */
#define WIFI_CONNECTED_BIT BIT0   // 连接成功标志位
#define WIFI_FAIL_BIT      BIT1   // 连接失败标志位

static const char *TAG = "wifi station";  // 日志标签

static int s_retry_num = 0;  // 当前重试次数


/**
 * @brief WiFi 事件处理函数
 * @param arg 传入的参数
 * @param event_base 事件基类（WIFI_EVENT 或 IP_EVENT）
 * @param event_id 事件 ID
 * @param event_data 事件数据
 */
static void event_handler(void* arg, esp_event_base_t event_base,
                                int32_t event_id, void* event_data)
{
    // WiFi 启动事件
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
        esp_wifi_connect();  // 开始连接 WiFi
    } 
    // WiFi 断开连接事件
    else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        // 如果未达到最大重试次数，则继续重试
        if (s_retry_num < EXAMPLE_ESP_MAXIMUM_RETRY) {
            esp_wifi_connect();  // 重新连接
            s_retry_num++;       // 重试次数加 1
            ESP_LOGI(TAG, "正在重试连接 AP");
        } else {
            // 达到最大重试次数，设置连接失败标志
            xEventGroupSetBits(s_wifi_event_group, WIFI_FAIL_BIT);
        }
        ESP_LOGI(TAG, "连接 AP 失败");
    } 
    // 获得 IP 地址事件
    else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t* event = (ip_event_got_ip_t*) event_data;
        ESP_LOGI(TAG, "获得 IP:" IPSTR, IP2STR(&event->ip_info.ip));
        s_retry_num = 0;  // 重置重试次数
        xEventGroupSetBits(s_wifi_event_group, WIFI_CONNECTED_BIT);  // 设置连接成功标志
    }
}

/**
 * @brief 初始化 WiFi 站点模式
 * 
 * 该函数完成以下步骤：
 * 1. 创建 FreeRTOS 事件组
 * 2. 初始化网络接口
 * 3. 创建默认事件循环
 * 4. 配置并启动 WiFi
 * 5. 等待连接成功或失败
 */
void wifi_init_sta(void)
{
    // 创建事件组
    s_wifi_event_group = xEventGroupCreate();

    // 初始化网络接口
    ESP_ERROR_CHECK(esp_netif_init());

    // 创建默认事件循环
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    
    // 创建默认的 WiFi 站点网络接口
    esp_netif_create_default_wifi_sta();

    // 获取默认的 WiFi 初始化配置
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    // 注册 WiFi 事件处理函数（处理任何 WiFi 事件）
    esp_event_handler_instance_t instance_any_id;
    esp_event_handler_instance_t instance_got_ip;
    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT,
                                                        ESP_EVENT_ANY_ID,
                                                        &event_handler,
                                                        NULL,
                                                        &instance_any_id));
    // 注册 IP 事件处理函数（处理获得 IP 事件）
    ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT,
                                                        IP_EVENT_STA_GOT_IP,
                                                        &event_handler,
                                                        NULL,
                                                        &instance_got_ip));

    // 配置 WiFi 站点模式参数
    wifi_config_t wifi_config = {
        .sta = {
            .ssid = "Licoe",       // WiFi 名称
            .password = "@Li123456789",  // WiFi 密码
            /* 认证模式阈值：如果密码符合 WPA2 标准（密码长度 >= 8），
             * 则重置为 WPA2。如果您想连接已弃用的 WEP/WPA 网络，
             * 请将阈值设置为 WIFI_AUTH_WEP/WIFI_AUTH_WPA_PSK，
             * 并设置符合 WIFI_AUTH_WEP/WIFI_AUTH_WPA_PSK 标准的密码长度和格式。
             */
            .threshold.authmode = ESP_WIFI_SCAN_AUTH_MODE_THRESHOLD,
            .sae_pwe_h2e = ESP_WIFI_SAE_MODE,
            .sae_h2e_identifier = EXAMPLE_H2E_IDENTIFIER,
#ifdef CONFIG_ESP_WIFI_WPA3_COMPATIBLE_SUPPORT
            .disable_wpa3_compatible_mode = 0,
#endif
        },
    };

    // 设置 WiFi 模式为站点模式
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA) );
    // 设置 WiFi 配置
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config) );
    // 启动 WiFi
    ESP_ERROR_CHECK(esp_wifi_start() );

    ESP_LOGI(TAG, "wifi_init_sta 已完成。");

    /* 等待连接建立（WIFI_CONNECTED_BIT）或达到最大重试次数（WIFI_FAIL_BIT）。
     * 这些位由 event_handler() 设置（见上文）*/
    EventBits_t bits = xEventGroupWaitBits(s_wifi_event_group,
            WIFI_CONNECTED_BIT | WIFI_FAIL_BIT,
            pdFALSE,
            pdFALSE,
            portMAX_DELAY);

    /* xEventGroupWaitBits() 返回调用返回前的位，因此我们可以测试实际发生了哪个事件。 */
    if (bits & WIFI_CONNECTED_BIT) {
        ESP_LOGI(TAG, "已连接到 AP，SSID:%s，密码:%s",
                 EXAMPLE_ESP_WIFI_SSID, EXAMPLE_ESP_WIFI_PASS);
    } else if (bits & WIFI_FAIL_BIT) {
        ESP_LOGI(TAG, "连接失败，SSID:%s，密码:%s",
                 EXAMPLE_ESP_WIFI_SSID, EXAMPLE_ESP_WIFI_PASS);
    } else {
        ESP_LOGE(TAG, "意外事件");
    }
}

/**
 * @brief 应用主函数
 * 
 * 程序入口点，完成以下初始化：
 * 1. 初始化 NVS（非易失性存储）
 * 2. 可选：设置 WiFi 模块日志级别
 * 3. 初始化并启动 WiFi 站点模式
 */
void app_main(void)
{
    // 初始化 NVS
    esp_err_t ret = nvs_flash_init();
    // 如果 NVS 没有空闲页或找到新版本，则擦除并重新初始化
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
      ESP_ERROR_CHECK(nvs_flash_erase());
      ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    // 如果需要更多日志（最大日志级别大于默认级别）
    if (CONFIG_LOG_MAXIMUM_LEVEL > CONFIG_LOG_DEFAULT_LEVEL) {
        /* 如果您只想在 wifi 模块中打开更多日志，需要将最大级别设置为大于默认级别，
         * 并在 esp_wifi_init() 之前调用 esp_log_level_set() 来提高 wifi 模块的日志级别。 */
        esp_log_level_set("wifi", CONFIG_LOG_MAXIMUM_LEVEL);
    }

    ESP_LOGI(TAG, "ESP_WIFI_MODE_STA");
    // 初始化 WiFi 站点模式并开始连接
    wifi_init_sta();
}
