idf_component_register(SRCS "weather.c" "weather_parse.c"
                       INCLUDE_DIRS "include"
                       PRIV_REQUIRES connectivity settings esp_http_client mbedtls esp_timer)
