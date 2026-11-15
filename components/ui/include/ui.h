#pragma once
#include "esp_err.h"
#include "lvgl.h"
#include "weather.h"
#include "sensor.h"
#include "connectivity.h"

typedef enum { UI_SCREEN_FACE, UI_SCREEN_WEATHER, UI_SCREEN_SENSOR } ui_screen_t;

/* All functions: LVGL context or under lvgl_port_lock(). Screens are created once. */
esp_err_t ui_init(void);
lv_obj_t *ui_face_parent(void);
void ui_show(ui_screen_t screen);          // fade or slide, per Kconfig
ui_screen_t ui_current(void);
void ui_update_weather(const weather_data_t *w, wifi_status_t wifi);
void ui_update_sensor(const sensor_data_t *s);
