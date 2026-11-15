/*
 * Desktop companion robot - ESP32-S3 N16R8 + ST7789 240x240 + DHT11 + Open-Meteo.
 *
 * Tasks (besides IDF's own Wi-Fi/lwIP/esp_timer):
 *   taskLVGL  core 1, prio 4  - esp_lvgl_port: rendering + every LVGL timer
 *                                (behaviour FSM, screen updates, monitor)
 *   dht11     core 1, prio 5  - DHT11 read every CONFIG_ROBOT_SENSOR_INTERVAL_MS
 *   weather   core 0, prio 3  - HTTPS fetch every CONFIG_ROBOT_WEATHER_UPDATE_INTERVAL_MIN
 * Sensor/weather publish snapshots behind a spinlock; only LVGL context touches LVGL.
 */
#include "nvs_flash.h"
#include "esp_idf_version.h"
#include "esp_log.h"
#include "esp_lvgl_port.h"
#include "display.h"
#include "ui.h"
#include "animation.h"
#include "sensor.h"
#include "connectivity.h"
#include "weather.h"
#include "app_controller.h"
#include "robot_settings.h"
#include "usb_control.h"

static const char *TAG = "main";

void app_main(void)
{
    ESP_LOGI(TAG, "Desktop robot starting (ESP-IDF %s)", esp_get_idf_version());

    esp_err_t err = nvs_flash_init();   // Wi-Fi needs NVS (PHY calibration data)
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        err = nvs_flash_init();
    }
    ESP_ERROR_CHECK(err);
    ESP_ERROR_CHECK(settings_init());

    // Display is the one hard requirement: fail loudly.
    ESP_ERROR_CHECK(display_check_pins());
    lv_display_t *disp = NULL;
    ESP_ERROR_CHECK(display_init(&disp));

    lvgl_port_lock(0);
    ESP_ERROR_CHECK(ui_init());
    ESP_ERROR_CHECK(animation_init(ui_face_parent()));
    lvgl_port_unlock();

    // Everything else is optional: the robot keeps animating offline / without a sensor.
    ESP_ERROR_CHECK_WITHOUT_ABORT(sensor_start());
    ESP_ERROR_CHECK_WITHOUT_ABORT(connectivity_start());
    ESP_ERROR_CHECK_WITHOUT_ABORT(weather_start());
    ESP_ERROR_CHECK(app_controller_start(disp));
    ESP_ERROR_CHECK_WITHOUT_ABORT(usb_control_start());
    ESP_LOGI(TAG, "running");
}
