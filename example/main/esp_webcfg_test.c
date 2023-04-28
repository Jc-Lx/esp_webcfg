/*
 * @Author: Jc-Lx 1031510595@qq.com
 * @Date: 2023-04-20 10:25:39
 * @LastEditors: Jc-Lx 1031510595@qq.com
 * @LastEditTime: 2023-04-26 14:45:54
 * @FilePath: \hello_world\main\hello_world_main.c
 * @Description: 这是默认设置,请设置`customMade`, 打开koroFileHeader查看配置 进行设置: https://github.com/OBKoro1/koro1FileHeader/wiki/%E9%85%8D%E7%BD%AE
 */
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_log.h"
#include "nvs_flash.h"

#include "lwip/err.h"
#include "lwip/sys.h"
#include "esp_spi_flash.h"
#include "esp_webcfg.h"

static const char* TAG =  "WEBCFG-AP";

void webcfg_handler(void* arg)
{
    webcfg_handle_t* handle = (webcfg_handle_t*) arg;
    
    switch (handle->uevt_id) {
        case ESP_WEBCFG_START:{
            ESP_LOGI(TAG,"ESP_WEBCFG START");
        }break;
        case ESP_WEBCFG_SENDINDEX:{
            ESP_LOGI(TAG,"ESP_WEBCFG_SENDINDEX");
        }break;
        case ESP_WEBCFG_UPDATE:{
            ESP_LOGI(TAG,"ESP_WEBCFG_UPDATE");
        }break;
        case ESP_WEBCFG_ONPARA:{
            /* Log data received */
            ESP_LOGI(TAG, "========= ESP_WEBCFG_ONPARA ========");
            ESP_LOGI(TAG, "%.*s", handle->ret , handle->buf);
            ESP_LOGI(TAG, "====================================");
        }break;
        case ESP_WEBCFG_STOP:{
            ESP_LOGI(TAG,"ESP_WEBCFG_STOP");
        }break;
        
        default:
            break;
    }
}

void wifi_ap_handler(void* arg, esp_event_base_t event_base,
                                    int32_t event_id, void* event_data)
{   
    if (event_base == WIFI_EVENT) {
        switch (event_id) {
            case WIFI_EVENT_AP_START:{
                ESP_LOGI(TAG,"AP started");
            };break;
            case WIFI_EVENT_AP_STACONNECTED:{
                wifi_event_ap_staconnected_t* event = (wifi_event_ap_staconnected_t*) event_data;
                ESP_LOGI(TAG, "station "MACSTR" join, AID=%d",
                                                MAC2STR(event->mac), event->aid);
                /* start ESP-WEBCFG */
                esp_webcfg_start(webcfg_handler);
            };break;
            case WIFI_EVENT_AP_STADISCONNECTED:{
                wifi_event_ap_stadisconnected_t* event = (wifi_event_ap_stadisconnected_t*) event_data;
                ESP_LOGI(TAG, "station "MACSTR" leave, AID=%d",
                                                MAC2STR(event->mac), event->aid);
                /* clean ESP-WEBCFG */
                esp_webcfg_clean();
            };break;        
            default:break;
        }
    } 
}

void app_main(void)
{
    printf("ESP-WEBCFG Test!\n");

    /* Print chip information */
    esp_chip_info_t chip_info;
    esp_chip_info(&chip_info);
    printf("This is %s chip with %d CPU core(s), WiFi%s%s, ",
            CONFIG_IDF_TARGET,
            chip_info.cores,
            (chip_info.features & CHIP_FEATURE_BT) ? "/BT" : "",
            (chip_info.features & CHIP_FEATURE_BLE) ? "/BLE" : "");

    printf("silicon revision %d, ", chip_info.revision);

    printf("%dMB %s flash\n", spi_flash_get_chip_size() / (1024 * 1024),
            (chip_info.features & CHIP_FEATURE_EMB_FLASH) ? "embedded" : "external");

    printf("Minimum free heap size: %d bytes\n", esp_get_minimum_free_heap_size());

    /* Initialize NVS */
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
      ESP_ERROR_CHECK(nvs_flash_erase());
      ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_t* netif = esp_netif_create_default_wifi_ap();
    esp_netif_ip_info_t netinfo;
    memset(&netinfo,0,sizeof(esp_netif_ip_info_t));
    if (netif != NULL){
        ESP_ERROR_CHECK(esp_netif_dhcps_stop(netif));
        netinfo.ip.addr = esp_ip4addr_aton((const char*)CONFIG_ESP_WEBCFG_IP);
        netinfo.gw.addr = esp_ip4addr_aton((const char*)CONFIG_ESP_WEBCFG_GW);
        netinfo.netmask.addr = esp_ip4addr_aton((const char*)CONFIG_ESP_WEBCFG_NETMASK);
        esp_netif_set_ip_info(netif,&netinfo);
        ESP_ERROR_CHECK(esp_netif_dhcps_start(netif));
    }
    
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));
    esp_event_handler_instance_t instance_any_id;
    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT,ESP_EVENT_ANY_ID,
                                                        &wifi_ap_handler,NULL,
                                                        &instance_any_id));

    wifi_config_t wifi_config = {
        .ap = {
            .ssid = CONFIG_ESP_WEBCFG_SSID,
            .ssid_len = strlen(CONFIG_ESP_WEBCFG_SSID),
            .channel = CONFIG_ESP_WEBCFG_WIFI_CHANNEL,
            .max_connection = CONFIG_ESP_WEBCFG_MAX_STA_CONN,
            .authmode = WIFI_AUTH_OPEN
        },
    };
    
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_AP));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_AP,&wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());
}
