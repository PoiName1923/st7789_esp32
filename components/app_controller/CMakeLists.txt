idf_component_register(SRCS "app_controller.c" "robot_command.c" "robot_fsm.c"
                       INCLUDE_DIRS "include"
                       REQUIRES lvgl__lvgl
                       PRIV_REQUIRES animation ui sensor weather connectivity settings
                                     espressif__esp_lvgl_port esp_timer esp_wifi)
