#include <string.h>
#include <stdlib.h>
#include "esp_wifi.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "esp_smartconfig.h"

#define TAG "NVS"

#define NVS_AP_INFO ("ap_info")
#define NVS_STRUCT ("ap_struct")

typedef struct
{
    uint8_t ssid[32];
    uint8_t passwd[64];
} ap_info_t;

static nvs_handle ap_info_handle;

/** 
 * @brief  load_wifi_info
 * @note   加载wifi信息
 * @retval 
 */
uint8_t load_wifi_info(wifi_config_t *config)
{
    ap_info_t nvs_info;
    esp_err_t err_code;

    memset(config,0,sizeof(wifi_config_t));
    
    err_code = nvs_open(NVS_AP_INFO, NVS_READWRITE, &ap_info_handle);
    if (err_code != ESP_OK)
    {
        ESP_LOGE(TAG, "Error (%d) opening NVS handle!\n", err_code);
        return 1;
    }
    else
    {
        size_t size = sizeof(ap_info_t);
        err_code = nvs_get_blob(ap_info_handle, NVS_STRUCT, &nvs_info, &size);
        if (err_code != ESP_OK)
        {
            ESP_LOGE(TAG, "Error (%d) reading!\n", err_code);
            return 1;
        }

        ESP_LOGI(TAG, "passwd:%s \n",nvs_info.passwd);
        ESP_LOGI(TAG, "ssid:%s \n",nvs_info.ssid);

        memcpy(config->sta.ssid, nvs_info.ssid, sizeof(nvs_info.ssid));
        memcpy(config->sta.password, nvs_info.passwd, sizeof(nvs_info.passwd));
        nvs_close(ap_info_handle);
    }
    return ESP_OK;
}

/** 
 * @brief  save_wifi_info
 * @note   
 * @param  *config: 
 * @retval 
 */
uint8_t save_wifi_info(wifi_config_t *config)
{
    ap_info_t nvs_info;
    esp_err_t err_code;
    err_code = nvs_open(NVS_AP_INFO, NVS_READWRITE, &ap_info_handle);
    if (err_code != ESP_OK)
    {
        ESP_LOGE(TAG, "Error (%d) opening NVS handle!\n", err_code);
        return 1;
    }
    else
    {
        size_t size = sizeof(ap_info_t);
        memcpy(nvs_info.ssid, config->sta.ssid, sizeof(config->sta.ssid));
        memcpy(nvs_info.passwd, config->sta.password, sizeof(config->sta.password));
        err_code = nvs_set_blob(ap_info_handle, NVS_STRUCT, &nvs_info, size);
        ESP_ERROR_CHECK(err_code);
        nvs_close(ap_info_handle);
    }
    return ESP_OK;
}
