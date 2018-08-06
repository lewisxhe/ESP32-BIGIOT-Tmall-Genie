#include <string.h>
#include <stdlib.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "sdkconfig.h"
#include "nvs_store.h"
#include "bigiot_platform.h"
#include "main.h"
#include "esp_wifi.h"
#include "esp_wpa2.h"
#include "esp_event_loop.h"
#include "esp_smartconfig.h"

#define TAG "main"

task_ctx_t p_ctx = {
    .q_handle = NULL,
    .event_group = NULL,
};

static EventGroupHandle_t wifi_event_group = NULL;

/** 
 * @brief  sc_callback
 * @note   
 * @param  status: 
 * @param  *pdata: 
 * @retval None
 */
static void sc_callback(smartconfig_status_t status, void *pdata)
{
    switch (status)
    {
    case SC_STATUS_WAIT:
        ESP_LOGI(TAG, "SC_STATUS_WAIT");
        break;
    case SC_STATUS_FIND_CHANNEL:
        ESP_LOGI(TAG, "SC_STATUS_FINDING_CHANNEL");
        break;
    case SC_STATUS_GETTING_SSID_PSWD:
        ESP_LOGI(TAG, "SC_STATUS_GETTING_SSID_PSWD");
        break;
    case SC_STATUS_LINK:
        ESP_LOGI(TAG, "SC_STATUS_LINK");
        wifi_config_t *wifi_config = pdata;
        ESP_LOGI(TAG, "SSID:%s", wifi_config->sta.ssid);
        ESP_LOGI(TAG, "PASSWORD:%s", wifi_config->sta.password);

        save_wifi_info(wifi_config);

        ESP_ERROR_CHECK(esp_wifi_disconnect());
        ESP_ERROR_CHECK(esp_wifi_set_config(ESP_IF_WIFI_STA, wifi_config));
        ESP_ERROR_CHECK(esp_wifi_connect());
        break;
    case SC_STATUS_LINK_OVER:
        ESP_LOGI(TAG, "SC_STATUS_LINK_OVER");
        if (pdata != NULL)
        {
            uint8_t phone_ip[4] = {0};
            memcpy(phone_ip, (uint8_t *)pdata, 4);
            ESP_LOGI(TAG, "ip: %d.%d.%d.%d\n", phone_ip[0], phone_ip[1], phone_ip[2], phone_ip[3]);
        }
        xEventGroupSetBits(wifi_event_group, ESPTOUCH_DONE_BIT);
        break;
    default:
        break;
    }
}

/** 
 * @brief  smartconfig_task
 * @note   
 * @param  *parm: 
 * @retval None
 */
static void smartconfig_task(void *parm)
{
    EventBits_t uxBits;
    ESP_ERROR_CHECK(esp_smartconfig_set_type(SC_TYPE_ESPTOUCH));
    ESP_ERROR_CHECK(esp_smartconfig_start(sc_callback));

    while (1)
    {
        uxBits = xEventGroupWaitBits(wifi_event_group, CONNECTED_BIT | ESPTOUCH_DONE_BIT, true, false, portMAX_DELAY);
        if (uxBits & CONNECTED_BIT)
        {
            ESP_LOGI(TAG, "WiFi Connected to ap");
        }
        if (uxBits & ESPTOUCH_DONE_BIT)
        {
            ESP_LOGI(TAG, "smartconfig over");
            esp_smartconfig_stop();
            vTaskDelete(NULL);
        }
    }
}

/** 
 * @brief  sys_event_handler
 * @note   
 * @param  *ctx: 
 * @param  *event: 
 * @retval 
 */
static esp_err_t sys_event_handler(void *ctx, system_event_t *event)
{
    switch (event->event_id)
    {
    case SYSTEM_EVENT_STA_CONNECTED:
        ESP_LOGI(TAG, "SYSTEM_EVENT_STA_CONNECTED\n");
        break;

    case SYSTEM_EVENT_STA_START:
        if (!(xEventGroupGetBits(wifi_event_group) & HAVE_WIFI_INFO_BIT))
        {
            ESP_LOGI(TAG, "no have passwd\n");
        }
        else
        {
            ESP_LOGI(TAG, "success have passwd\n");
            ESP_ERROR_CHECK(esp_wifi_connect());
        }
        break;

    case SYSTEM_EVENT_STA_GOT_IP:
        ESP_LOGI(TAG, "SYSTEM_EVENT_STA_GOT_IP\n");
        xEventGroupSetBits(wifi_event_group, CONNECTED_BIT);
        break;

    case SYSTEM_EVENT_STA_DISCONNECTED:
        ESP_LOGI(TAG, "SYSTEM_EVENT_STA_DISCONNECTED\n");
        esp_wifi_connect();
        xEventGroupClearBits(wifi_event_group, CONNECTED_BIT);
        break;

    default:
        break;
    }
    return ESP_OK;
}

/** 
 * @brief  init_wifi
 * @note   
 * @retval None
 */
void init_wifi(void *parm)
{
    esp_err_t err_code;
    tcpip_adapter_init();

    wifi_event_group = ((task_ctx_t *)parm)->event_group;

    err_code = esp_event_loop_init(sys_event_handler, NULL);
    ESP_ERROR_CHECK(err_code);

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));

    wifi_config_t config;
    err_code = load_wifi_info(&config);

    if (err_code == ESP_OK)
    {
        xEventGroupSetBits(wifi_event_group, HAVE_WIFI_INFO_BIT);
        ESP_ERROR_CHECK(esp_wifi_set_config(ESP_IF_WIFI_STA, &config));
        ESP_ERROR_CHECK(esp_wifi_start());
    }
    else
    {
        ESP_ERROR_CHECK(esp_wifi_start());
        xTaskCreate(smartconfig_task, "smartconfig_task", 4096, NULL, 3, NULL);
    }
}

/** 
 * @brief  event_handler_task
 * @note   
 * @param  *parm: 
 * @retval None
 */
void event_handler_task(void *parm)
{
    task_ctx_t *ctx = parm;
    EventBits_t uxBits;
    while (1)
    {
        uxBits = xEventGroupWaitBits(ctx->event_group, CONNECTED_BIT, true, false, portMAX_DELAY);
        if (uxBits & CONNECTED_BIT)
        {
            ets_printf("WiFi Connected to ap\n");
            xTaskCreate(BIGIOT_Task, "update_task", 4096, NULL, 3, NULL);
            vTaskDelete(NULL);
        }
    }
}

/** 
 * @brief  app_main
 * @note   
 * @retval None
 */
void app_main()
{
    esp_err_t err_code;

    err_code = nvs_flash_init();
    if (err_code == ESP_ERR_NVS_NO_FREE_PAGES)
    {
        ESP_ERROR_CHECK(nvs_flash_erase());
        err_code = nvs_flash_init();
    }
    ESP_ERROR_CHECK(err_code);

    //组事件管理
    p_ctx.event_group = xEventGroupCreate();
    APP_ERROR_CHECK(!p_ctx.event_group, "xEventGroupCreate failed", while (1));

    init_wifi(&p_ctx);

    xTaskCreate(event_handler_task, "event_handler", 4096, &p_ctx, 3, NULL);
}
