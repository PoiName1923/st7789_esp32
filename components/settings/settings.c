#include "robot_settings.h"
#include "nvs.h"
#include "esp_random.h"
#include "esp_check.h"
#include "mbedtls/md.h"
#include "mbedtls/pkcs5.h"
#include "freertos/FreeRTOS.h"
#include <string.h>

#define SETTINGS_VERSION 1
#define ADMIN_SALT_LEN 16
#define ADMIN_HASH_LEN 32
#define PBKDF2_ROUNDS 100000

typedef struct { uint32_t version; robot_settings_t value; } stored_settings_t;
typedef struct { uint32_t version; uint8_t salt[ADMIN_SALT_LEN]; uint8_t hash[ADMIN_HASH_LEN]; } stored_admin_t;
static const char *TAG = "settings";
static portMUX_TYPE s_lock = portMUX_INITIALIZER_UNLOCKED;
static robot_settings_t s_settings;
static uint8_t s_admin_salt[ADMIN_SALT_LEN], s_admin_hash[ADMIN_HASH_LEN];

static esp_err_t derive(const char *password, const uint8_t *salt, uint8_t *out)
{
    if (!password || !password[0]) return ESP_ERR_INVALID_ARG;
    return mbedtls_pkcs5_pbkdf2_hmac_ext(MBEDTLS_MD_SHA256,
        (const unsigned char *)password, strlen(password), salt, ADMIN_SALT_LEN,
        PBKDF2_ROUNDS, ADMIN_HASH_LEN, out) == 0 ? ESP_OK : ESP_FAIL;
}

static bool secure_equal(const uint8_t *a, const uint8_t *b, size_t n)
{
    uint8_t diff = 0;
    for (size_t i = 0; i < n; i++) diff |= a[i] ^ b[i];
    return diff == 0;
}

static esp_err_t save_blob(const char *key, const void *value, size_t size, bool commit)
{
    nvs_handle_t nvs;
    esp_err_t err = nvs_open("robot", NVS_READWRITE, &nvs);
    if (err != ESP_OK) return err;
    err = nvs_set_blob(nvs, key, value, size);
    if (err == ESP_OK && commit) err = nvs_commit(nvs);
    nvs_close(nvs);
    return err;
}

static esp_err_t save_settings(void)
{
    stored_settings_t stored = { .version = SETTINGS_VERSION, .value = s_settings };
    return save_blob("settings", &stored, sizeof(stored), true);
}

static esp_err_t save_admin(void)
{
    stored_admin_t admin = { .version = SETTINGS_VERSION };
    memcpy(admin.salt, s_admin_salt, sizeof(admin.salt));
    memcpy(admin.hash, s_admin_hash, sizeof(admin.hash));
    return save_blob("admin", &admin, sizeof(admin), true);
}

esp_err_t settings_init(void)
{
    settings_defaults(&s_settings);
    strlcpy(s_settings.wifi_ssid, CONFIG_ROBOT_WIFI_SSID, sizeof(s_settings.wifi_ssid));
    strlcpy(s_settings.wifi_password, CONFIG_ROBOT_WIFI_PASSWORD, sizeof(s_settings.wifi_password));
    strlcpy(s_settings.ap_ssid, CONFIG_ROBOT_AP_SSID, sizeof(s_settings.ap_ssid));
    strlcpy(s_settings.ap_password, CONFIG_ROBOT_AP_PASSWORD, sizeof(s_settings.ap_password));
    strlcpy(s_settings.weather_latitude, CONFIG_ROBOT_WEATHER_LATITUDE, sizeof(s_settings.weather_latitude));
    strlcpy(s_settings.weather_longitude, CONFIG_ROBOT_WEATHER_LONGITUDE, sizeof(s_settings.weather_longitude));
    strlcpy(s_settings.weather_timezone, CONFIG_ROBOT_WEATHER_TIMEZONE, sizeof(s_settings.weather_timezone));
    s_settings.animation_fps = CONFIG_ROBOT_ANIMATION_DEFAULT_FPS;
    s_settings.screen_switch_s = CONFIG_ROBOT_SCREEN_SWITCH_INTERVAL_S;
    s_settings.info_screen_s = CONFIG_ROBOT_INFO_SCREEN_DURATION_S;
    s_settings.display_rotation = CONFIG_ROBOT_DISPLAY_ROTATION;
#ifdef CONFIG_ROBOT_DISPLAY_BGR
    s_settings.display_bgr = true;
#else
    s_settings.display_bgr = false;
#endif
#ifdef CONFIG_ROBOT_LCD_INVERT
    s_settings.display_invert = true;
#else
    s_settings.display_invert = false;
#endif
    s_settings.display_offset_x = CONFIG_ROBOT_LCD_OFFSET_X;
    s_settings.display_offset_y = CONFIG_ROBOT_LCD_OFFSET_Y;
    s_settings.weather_interval_min = CONFIG_ROBOT_WEATHER_UPDATE_INTERVAL_MIN;

    nvs_handle_t nvs;
    ESP_RETURN_ON_ERROR(nvs_open("robot", NVS_READWRITE, &nvs), TAG, "open NVS");
    stored_settings_t stored;
    size_t size = sizeof(stored);
    if (nvs_get_blob(nvs, "settings", &stored, &size) == ESP_OK && size == sizeof(stored) &&
        stored.version == SETTINGS_VERSION && settings_validate(&stored.value) == SETTINGS_OK)
        s_settings = stored.value;
    // Old installations may have AUTO persisted; retain their settings but pin the face.
    if (s_settings.screen_mode == ROBOT_SCREEN_AUTO) s_settings.screen_mode = ROBOT_SCREEN_FACE;
    stored_admin_t admin;
    size = sizeof(admin);
    bool have_admin = nvs_get_blob(nvs, "admin", &admin, &size) == ESP_OK && size == sizeof(admin) &&
                      admin.version == SETTINGS_VERSION;
    if (have_admin) { memcpy(s_admin_salt, admin.salt, sizeof(s_admin_salt)); memcpy(s_admin_hash, admin.hash, sizeof(s_admin_hash)); }
    nvs_close(nvs);
    if (!have_admin && CONFIG_ROBOT_ADMIN_PASSWORD[0]) {
        esp_fill_random(s_admin_salt, sizeof(s_admin_salt));
        ESP_RETURN_ON_ERROR(derive(CONFIG_ROBOT_ADMIN_PASSWORD, s_admin_salt, s_admin_hash), TAG, "admin password");
        ESP_RETURN_ON_ERROR(save_admin(), TAG, "save admin");
    }
    if (settings_validate(&s_settings) != SETTINGS_OK) return ESP_ERR_INVALID_STATE;
    return save_settings();
}

void settings_get(robot_settings_t *out)
{
    if (!out) return;
    portENTER_CRITICAL(&s_lock); *out = s_settings; portEXIT_CRITICAL(&s_lock);
}

esp_err_t settings_update(const robot_settings_patch_t *patch, settings_change_t *change)
{
    robot_settings_t next;
    settings_get(&next);
    if (settings_apply_patch(&next, patch, change) != SETTINGS_OK) return ESP_ERR_INVALID_ARG;
    portENTER_CRITICAL(&s_lock); robot_settings_t previous = s_settings; s_settings = next; portEXIT_CRITICAL(&s_lock);
    esp_err_t err = save_settings();
    if (err != ESP_OK) { portENTER_CRITICAL(&s_lock); s_settings = previous; portEXIT_CRITICAL(&s_lock); }
    return err;
}

bool settings_verify_admin(const char *password)
{
    uint8_t candidate[ADMIN_HASH_LEN];
    return derive(password, s_admin_salt, candidate) == ESP_OK && secure_equal(candidate, s_admin_hash, sizeof(candidate));
}

esp_err_t settings_change_admin(const char *old_password, const char *new_password)
{
    if (!settings_verify_admin(old_password) || !new_password || strlen(new_password) < 10) return ESP_ERR_INVALID_ARG;
    uint8_t salt[ADMIN_SALT_LEN], hash[ADMIN_HASH_LEN];
    esp_fill_random(salt, sizeof(salt));
    ESP_RETURN_ON_ERROR(derive(new_password, salt, hash), TAG, "derive admin");
    stored_admin_t admin = { .version = SETTINGS_VERSION };
    memcpy(admin.salt, salt, sizeof(salt)); memcpy(admin.hash, hash, sizeof(hash));
    esp_err_t err = save_blob("admin", &admin, sizeof(admin), true);
    if (err == ESP_OK) { memcpy(s_admin_salt, salt, sizeof(salt)); memcpy(s_admin_hash, hash, sizeof(hash)); }
    return err;
}
