idf_component_register(SRCS "app_main.c"
                       INCLUDE_DIRS "."
                       PRIV_REQUIRES nvs_flash espressif__esp_lvgl_port
                                     display animation sensor connectivity weather ui app_controller settings usb_control)
