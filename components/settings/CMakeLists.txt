idf_component_register(SRCS "settings.c" "settings_validate.c"
                       INCLUDE_DIRS "include"
                       REQUIRES nvs_flash mbedtls)
