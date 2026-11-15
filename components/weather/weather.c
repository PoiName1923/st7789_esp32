#include <stdio.h>
#include "weather.h"
#include "connectivity.h"
#include "esp_http_client.h"
#include "esp_crt_bundle.h"
#include "esp_timer.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "robot_settings.h"

#define RETRY_MS       60000
#define HTTP_TIMEOUT_MS 10000

static const char *TAG = "weather";
static portMUX_TYPE s_lock = portMUX_INITIALIZER_UNLOCKED;
static weather_data_t s_data;
static char s_body[2048];     // response is ~600 B; static so no per-fetch allocation

static esp_err_t fetch_once(void)
{
    robot_settings_t settings;
    settings_get(&settings);
    char url[256];
    snprintf(url, sizeof(url),
             "https://api.open-meteo.com/v1/forecast?latitude=%s&longitude=%s"
             "&current=temperature_2m,relative_humidity_2m,weather_code,is_day,wind_speed_10m"
             "&timezone=%s",
             settings.weather_latitude, settings.weather_longitude, settings.weather_timezone);

    esp_http_client_config_t cfg = {
        .url = url,
        .crt_bundle_attach = esp_crt_bundle_attach,   // verify server cert against IDF CA bundle
        .timeout_ms = HTTP_TIMEOUT_MS,
    };
    esp_http_client_handle_t client = esp_http_client_init(&cfg);
    if (!client) return ESP_ERR_NO_MEM;

    esp_err_t err = esp_http_client_open(client, 0);
    int status = 0, len = 0;
    if (err == ESP_OK) {
        esp_http_client_fetch_headers(client);
        status = esp_http_client_get_status_code(client);
        len = esp_http_client_read_response(client, s_body, sizeof(s_body) - 1);
    }
    esp_http_client_close(client);
    esp_http_client_cleanup(client);

    if (err != ESP_OK) { ESP_LOGW(TAG, "connect failed: %s", esp_err_to_name(err)); return err; }
    if (status != 200 || len <= 0) { ESP_LOGW(TAG, "HTTP %d, %d bytes", status, len); return ESP_FAIL; }
    s_body[len] = '\0';

    weather_current_t cur;
    if (weather_parse_open_meteo(s_body, &cur) != 0) {
        ESP_LOGW(TAG, "unexpected response: %.120s", s_body);
        return ESP_ERR_INVALID_RESPONSE;
    }
    portENTER_CRITICAL(&s_lock);
    s_data.cur = cur;
    s_data.valid = true;
    s_data.updated_ms = esp_timer_get_time() / 1000;
    portEXIT_CRITICAL(&s_lock);
    ESP_LOGI(TAG, "%.1f C, %.0f%%, code %d (%s), wind %.1f km/h, %s",
             cur.temperature, cur.humidity, cur.weather_code, weather_code_text(cur.weather_code),
             cur.wind_speed, cur.time);
    return ESP_OK;
}

static void weather_task(void *arg)
{
    for (;;) {
        if (!connectivity_wait(60000)) { vTaskDelay(pdMS_TO_TICKS(1000)); continue; }
        bool ok = fetch_once() == ESP_OK;
        robot_settings_t settings;
        settings_get(&settings);
        int64_t interval_ms = settings.weather_interval_min * 60 * 1000LL;
        vTaskDelay((TickType_t)((ok ? interval_ms : RETRY_MS) / portTICK_PERIOD_MS));
    }
}

esp_err_t weather_start(void)
{
#if !CONFIG_ROBOT_ENABLE_WEATHER
    return ESP_OK;
#endif
    // TLS handshake needs a deep stack.
    BaseType_t ok = xTaskCreatePinnedToCore(weather_task, "weather", 8192, NULL, 3, NULL, 0);
    return ok == pdPASS ? ESP_OK : ESP_ERR_NO_MEM;
}

void weather_get(weather_data_t *out)
{
    robot_settings_t settings;
    settings_get(&settings);
    portENTER_CRITICAL(&s_lock);
    *out = s_data;
    portEXIT_CRITICAL(&s_lock);
    out->stale = !out->valid || (esp_timer_get_time() / 1000 - out->updated_ms) >
                 2 * settings.weather_interval_min * 60 * 1000LL;
}
