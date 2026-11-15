idf_component_register(SRCS "display.c" "display_orient.c" "board_pins.c"
                       INCLUDE_DIRS "include"
                       REQUIRES lvgl__lvgl espressif__esp_lvgl_port esp_lcd
                       PRIV_REQUIRES esp_driver_spi esp_driver_gpio settings)
