idf_component_register(SRCS "sensor.c" "dht11_proto.c"
                       INCLUDE_DIRS "include"
                       PRIV_REQUIRES esp_driver_gpio esp_timer)
