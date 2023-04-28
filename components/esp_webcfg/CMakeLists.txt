idf_component_register(SRCS  "src/esp_webcfg.c"
                       INCLUDE_DIRS "include"
                       EMBED_TXTFILES web/index.html
                       REQUIRES nvs_flash esp_http_server app_update esp_netif)
