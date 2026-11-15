idf_component_register(SRCS "usb_control.c" INCLUDE_DIRS "include"
                       PRIV_REQUIRES app_controller settings sensor connectivity animation esp_hw_support
                                     driver json mbedtls esp_driver_usb_serial_jtag)
