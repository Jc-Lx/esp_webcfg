/*
 * @Author: Jc-Lx 1031510595@qq.com
 * @Date: 2023-04-20 10:38:35
 * @LastEditors: Jc-Lx 1031510595@qq.com
 * @LastEditTime: 2023-04-26 11:14:10
 * @FilePath: \example\components\esp_webcfg\include\esp_webcfg.h
 * @Description: 这是默认设置,请设置`customMade`, 打开koroFileHeader查看配置 进行设置: https://github.com/OBKoro1/koro1FileHeader/wiki/%E9%85%8D%E7%BD%AE
 */
#pragma once
#ifndef __ESP_WEBCFG_H_
#define __ESP_WEBCFG_H_

#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "esp_log.h"
#include "nvs_flash.h"

#include "esp_netif.h"
#include "lwip/err.h"
#include "lwip/sys.h"
#include "esp_http_server.h"
#include "esp_ota_ops.h"

#ifdef _cplusplus
extern "c"{
#endif

#define CACHESIZE       1024
#define MIN(a,b) ((a)<(b)?(a):(b))

typedef enum {
    ESP_WEBCFG_START,
    ESP_WEBCFG_SENDINDEX,
    ESP_WEBCFG_UPDATE,
    ESP_WEBCFG_ONPARA,
    ESP_WEBCFG_STOP
}webcfg_event_t;

typedef void (*webcfg_event_handle_fun)(void* arg);

typedef struct webcfg {
    char                        buf[CACHESIZE];              /* data cache */
    int32_t                     ret;                         /* data len and httpd err */
    webcfg_event_t              uevt_id;                     /* for user handle event */
    webcfg_event_handle_fun     evt_handle_cb;
    httpd_handle_t              server;
}webcfg_handle_t;

void esp_webcfg_clean(void);

esp_err_t esp_webcfg_start(webcfg_event_handle_fun fun);

#ifdef _cplusplus
}
#endif
#endif
