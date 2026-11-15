#include <string.h>
#include "connectivity.h"
#include "backoff.h"
#include "esp_wifi.h"
#include "esp_netif.h"
#include "esp_event.h"
#include "esp_timer.h"
#include "esp_check.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "robot_settings.h"

#define CONNECTED_BIT BIT0
#define BACKOFF_BASE_MS 1000

static const char *TAG = "wifi";
static EventGroupHandle_t s_events;
static esp_timer_handle_t s_retry_timer;
static volatile wifi_status_t s_status = WIFI_STATUS_DISABLED;
static uint32_t s_attempt;
static connectivity_state_t s_state;
static esp_netif_t *s_sta_netif;
static char s_sta_ip[16], s_ap_ip[16];
static bool s_sta_enabled, s_reconfiguring;

static void retry_cb(void *arg) { if (s_sta_enabled && !s_reconfiguring) esp_wifi_connect(); }

static void copy_ssid(uint8_t out[32], const char *value)
{
    size_t n = strlen(value);
    if (n > 32) n = 32;
    memset(out, 0, 32);
    memcpy(out, value, n);
}

static void on_event(void *arg, esp_event_base_t base, int32_t id, void *data)
{
    if (base == WIFI_EVENT && id == WIFI_EVENT_STA_START) {
        if (s_sta_enabled) esp_wifi_connect();
    } else if (base == WIFI_EVENT && id == WIFI_EVENT_STA_DISCONNECTED) {
        xEventGroupClearBits(s_events, CONNECTED_BIT);
        if (s_reconfiguring || !s_sta_enabled) return;
        s_status = WIFI_STATUS_CONNECTING;
        uint32_t delay = backoff_delay_ms(s_attempt++, BACKOFF_BASE_MS, CONFIG_ROBOT_WIFI_BACKOFF_MAX_S * 1000U);
        const wifi_event_sta_disconnected_t *d = data;
        connectivity_state_event(&s_state, CONNECTIVITY_EVENT_STA_DISCONNECTED, d->reason);
        ESP_LOGW(TAG, "disconnected (reason %d), retry #%lu in %lu ms", d->reason,
                 (unsigned long)s_attempt, (unsigned long)delay);
        esp_timer_stop(s_retry_timer);   // harmless if not running
        esp_timer_start_once(s_retry_timer, (uint64_t)delay * 1000);
    } else if (base == IP_EVENT && id == IP_EVENT_STA_GOT_IP) {
        const ip_event_got_ip_t *e = data;
        ESP_LOGI(TAG, "connected, ip " IPSTR, IP2STR(&e->ip_info.ip));
        s_attempt = 0;
        s_status = WIFI_STATUS_CONNECTED;
        connectivity_state_event(&s_state, CONNECTIVITY_EVENT_STA_GOT_IP, 0);
        snprintf(s_sta_ip, sizeof(s_sta_ip), IPSTR, IP2STR(&e->ip_info.ip));
        xEventGroupSetBits(s_events, CONNECTED_BIT);
    }
}

esp_err_t connectivity_start(void)
{
    s_events = xEventGroupCreate();
    ESP_RETURN_ON_FALSE(s_events, ESP_ERR_NO_MEM, TAG, "event group");
    robot_settings_t settings;
    settings_get(&settings);
    connectivity_state_init(&s_state, settings.wifi_ssid[0] != '\0');
    s_sta_enabled = settings.wifi_ssid[0] != '\0';

    const esp_timer_create_args_t targs = { .callback = retry_cb, .name = "wifi_retry" };
    ESP_RETURN_ON_ERROR(esp_timer_create(&targs, &s_retry_timer), TAG, "timer");
    ESP_RETURN_ON_ERROR(esp_netif_init(), TAG, "netif");
    esp_err_t err = esp_event_loop_create_default();
    ESP_RETURN_ON_FALSE(err == ESP_OK || err == ESP_ERR_INVALID_STATE, err, TAG, "event loop");
    s_sta_netif = esp_netif_create_default_wifi_sta();

    wifi_init_config_t init = WIFI_INIT_CONFIG_DEFAULT();
    ESP_RETURN_ON_ERROR(esp_wifi_init(&init), TAG, "wifi init");
    ESP_RETURN_ON_ERROR(esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, on_event, NULL), TAG, "evt");
    ESP_RETURN_ON_ERROR(esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, on_event, NULL), TAG, "evt");

    wifi_config_t sta = { 0 };
    copy_ssid(sta.sta.ssid, settings.wifi_ssid);
    strlcpy((char *)sta.sta.password, settings.wifi_password, sizeof(sta.sta.password));
    sta.sta.threshold.authmode = settings.wifi_password[0] ? WIFI_AUTH_WPA2_PSK : WIFI_AUTH_OPEN;

    ESP_RETURN_ON_ERROR(esp_wifi_set_storage(WIFI_STORAGE_RAM), TAG, "storage");
    ESP_RETURN_ON_ERROR(esp_wifi_set_mode(WIFI_MODE_STA), TAG, "mode");
    if (settings.wifi_ssid[0]) ESP_RETURN_ON_ERROR(esp_wifi_set_config(WIFI_IF_STA, &sta), TAG, "STA config");
    s_status = settings.wifi_ssid[0] ? WIFI_STATUS_CONNECTING : WIFI_STATUS_DISABLED;
    ESP_RETURN_ON_ERROR(esp_wifi_start(), TAG, "start");
    ESP_LOGI(TAG, "weather Wi-Fi %s", settings.wifi_ssid[0] ? "connecting" : "not configured");
    return ESP_OK;
}

wifi_status_t connectivity_status(void) { return s_status; }

const char *connectivity_status_str(wifi_status_t s)
{
    switch (s) {
    case WIFI_STATUS_DISABLED:   return "offline";
    case WIFI_STATUS_CONNECTING: return "connecting";
    case WIFI_STATUS_CONNECTED:  return "online";
    }
    return "?";
}

bool connectivity_wait(uint32_t timeout_ms)
{
    if (s_status == WIFI_STATUS_DISABLED) return false;
    return xEventGroupWaitBits(s_events, CONNECTED_BIT, pdFALSE, pdTRUE, pdMS_TO_TICKS(timeout_ms)) & CONNECTED_BIT;
}

esp_err_t connectivity_apply_settings(void)
{
    robot_settings_t settings;
    settings_get(&settings);
    wifi_config_t sta = {0};
    copy_ssid(sta.sta.ssid, settings.wifi_ssid);
    strlcpy((char *)sta.sta.password, settings.wifi_password, sizeof(sta.sta.password));
    sta.sta.threshold.authmode = settings.wifi_password[0] ? WIFI_AUTH_WPA2_PSK : WIFI_AUTH_OPEN;
    s_reconfiguring = true;
    esp_timer_stop(s_retry_timer);
    esp_wifi_disconnect();
    xEventGroupClearBits(s_events, CONNECTED_BIT);
    esp_err_t err;
    s_sta_enabled = settings.wifi_ssid[0] != '\0';
    if (!s_sta_enabled) {
        s_status = WIFI_STATUS_DISABLED;
        connectivity_state_init(&s_state, false);
        s_sta_ip[0] = '\0';
        s_reconfiguring = false;
        return ESP_OK;
    }
    err = esp_wifi_set_config(WIFI_IF_STA, &sta);
    if (err != ESP_OK) { s_reconfiguring = false; return err; }
    connectivity_state_event(&s_state, CONNECTIVITY_EVENT_CREDENTIALS_CHANGED, 0);
    s_status = WIFI_STATUS_CONNECTING;
    s_reconfiguring = false;
    return esp_wifi_connect();
}

void connectivity_get_info(connectivity_info_t *out)
{
    if (!out) return;
    *out = (connectivity_info_t){ .state = s_state };
    strlcpy(out->sta_ip, s_sta_ip, sizeof(out->sta_ip));
    strlcpy(out->ap_ip, s_ap_ip, sizeof(out->ap_ip));
}
