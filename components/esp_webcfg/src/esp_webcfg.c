/*
 * @Author: Jc-Lx 1031510595@qq.com
 * @Date: 2023-04-20 10:38:35
 * @LastEditors: Jc-Lx 1031510595@qq.com
 * @LastEditTime: 2023-04-26 15:00:43
 * @FilePath: \example\components\esp_webcfg\src\esp_webcfg.c
 * @Description: 这是默认设置,请设置`customMade`, 打开koroFileHeader查看配置 进行设置: https://github.com/OBKoro1/koro1FileHeader/wiki/%E9%85%8D%E7%BD%AE
 */
#include "esp_webcfg.h"

extern const uint8_t index_html_start[] asm("_binary_index_html_start");
extern const uint8_t index_html_end[] asm("_binary_index_html_end");

/**
 * @brief: Serve OTA update portal (index.html)
 * @param {httpd_req_t} *req
 * @return {*}
 */
static esp_err_t esp_webcfg_dispatch_event(webcfg_handle_t* handle, 
                                                        webcfg_event_t evt_id)
{   
    if (!handle || !handle->evt_handle_cb) {
        return ESP_FAIL;
    }

    handle->uevt_id = evt_id;
    handle->evt_handle_cb(handle);
    return ESP_OK;
}

/*
 * Handle send index file upload
 */
static esp_err_t index_get_handler(httpd_req_t *req)
{
    webcfg_handle_t* handle = (webcfg_handle_t* )req->user_ctx;

	httpd_resp_send(req, (const char *) index_html_start, index_html_end - index_html_start);
    
    esp_webcfg_dispatch_event(handle,ESP_WEBCFG_SENDINDEX);
	return ESP_OK;
}

/*
 * Handle update upload
 */
static esp_err_t update_post_handler(httpd_req_t *req)
{
    webcfg_handle_t* handle = (webcfg_handle_t* )req->user_ctx;
    memset(handle->buf,0,1024);
    handle->ret = 0;

	esp_ota_handle_t ota_handle;
	int remaining = req->content_len;

	const esp_partition_t *ota_partition = esp_ota_get_next_update_partition(NULL);
	ESP_ERROR_CHECK(esp_ota_begin(ota_partition, OTA_SIZE_UNKNOWN, &ota_handle));

    esp_webcfg_dispatch_event(handle,ESP_WEBCFG_UPDATE);
	while (remaining > 0) {
		handle->ret = httpd_req_recv(req, handle->buf, MIN(remaining,CACHESIZE));

		// Timeout Error: Just retry
		if (handle->ret == HTTPD_SOCK_ERR_TIMEOUT) {
			continue;

		// Serious Error: Abort OTA
		} else if (handle->ret <= 0) {
			httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Protocol Error");
			return ESP_FAIL;
		}

		// Successful Upload: Flash firmware chunk
		if (esp_ota_write(ota_handle, (const void *)handle->buf, handle->ret) != ESP_OK) {
			httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Flash Error");
			return ESP_FAIL;
		}

		remaining -= handle->ret;
	}

	// Validate and switch to new OTA image and reboot
	if (esp_ota_end(ota_handle) != ESP_OK || esp_ota_set_boot_partition(ota_partition) != ESP_OK) {
			httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Validation / Activation Error");
			return ESP_FAIL;
	}

	httpd_resp_sendstr(req, "Firmware update complete, rebooting now!\n");

	esp_restart();

	return ESP_OK;
}

/*
 * Handle parameter upload
 */
static esp_err_t parameter_post_handler(httpd_req_t *req)
{
    webcfg_handle_t* handle = (webcfg_handle_t* )req->user_ctx;
    memset(handle->buf,0,1024);
    handle->ret = 0;

    int remaining = req->content_len;
    while (remaining > 0) {
        /* Read the data for the request */
        if ((handle->ret  = httpd_req_recv(req, handle->buf, MIN(remaining, CACHESIZE))) <= 0) {
            if (handle->ret  == HTTPD_SOCK_ERR_TIMEOUT) {
                /* Retry receiving if timeout occurred */
                continue;
            }
            return ESP_FAIL;
        }

        /* Send back the same data */
        httpd_resp_send_chunk(req, handle->buf, handle->ret );
        remaining -= handle->ret ;

        esp_webcfg_dispatch_event(handle,ESP_WEBCFG_ONPARA);

    }

    /* End response */
    httpd_resp_send_chunk(req, NULL, 0);
    return ESP_OK;
}

static TaskHandle_t esp_webcfg = NULL;
static webcfg_handle_t handle;
static void esp_webcfg_task(void* arg)
{
    /* set webcfg handle */
    handle.evt_handle_cb = (webcfg_event_handle_fun) arg;

    /* start httpd */
    //httpd_handle_t server = NULL;
    httpd_config_t cfg = HTTPD_DEFAULT_CONFIG();
    if (httpd_start(&handle.server,&cfg) != ESP_OK) {
        vTaskDelete(esp_webcfg);
        return;
    }

    /* httpd register index_post handle */
    httpd_uri_t index_get = {
        .uri	  = "/",
        .method   = HTTP_GET,
        .handler  = index_get_handler,
        .user_ctx = &handle
    };
    if (httpd_register_uri_handler(handle.server, &index_get) != ESP_OK) {
        vTaskDelete(esp_webcfg);
        return;
    }

    /* httpd register update_post handle */
    httpd_uri_t update_post = {
        .uri	  = "/update",
	    .method   = HTTP_POST,
	    .handler  = update_post_handler,
	    .user_ctx = &handle
    };
    if (httpd_register_uri_handler(handle.server, &update_post) != ESP_OK) {
        vTaskDelete(esp_webcfg);
        return;
    }

    /* httpd register parameter_post handle */
    httpd_uri_t parameter_post = {
        .uri	  = "/parameter",
        .method   = HTTP_POST,
        .handler  = parameter_post_handler,
        .user_ctx = &handle
    };
    if (httpd_register_uri_handler(handle.server, &parameter_post) != ESP_OK) {
        vTaskDelete(esp_webcfg);
        return;
    }
    
    esp_webcfg_dispatch_event(&handle,ESP_WEBCFG_START);
    for (;;) {
        vTaskDelay(1000 / portTICK_PERIOD_MS);
    } 
}

void esp_webcfg_clean(void)
{
    httpd_stop(handle.server);
    vTaskDelete(esp_webcfg);
}

esp_err_t esp_webcfg_start(webcfg_event_handle_fun fun)
{
    BaseType_t err =  xTaskCreatePinnedToCore(esp_webcfg_task,"esp_webcfg_task",
                                                            4096,fun,3,&esp_webcfg,0);
    return err;
}

