#pragma once
#include <stdbool.h>
#include <stdint.h>

#define ROBOT_SSID_MAX 32
#define ROBOT_WIFI_PASSWORD_MAX 63
#define ROBOT_AP_SSID_MAX 32
#define ROBOT_TIMEZONE_MAX 63

typedef enum {
    ROBOT_SCREEN_AUTO,
    ROBOT_SCREEN_FACE,
    ROBOT_SCREEN_WEATHER,
    ROBOT_SCREEN_SENSOR,
} robot_screen_mode_t;

typedef enum {
    SETTINGS_OK,
    SETTINGS_ERR_ARGUMENT,
    SETTINGS_ERR_AP_PASSWORD,
    SETTINGS_ERR_WIFI,
    SETTINGS_ERR_DISPLAY,
    SETTINGS_ERR_ANIMATION,
    SETTINGS_ERR_WEATHER,
} settings_result_t;

typedef struct {
    robot_screen_mode_t screen_mode;
    uint8_t animation_id;
    uint8_t animation_fps;
    uint32_t face_color;
    uint16_t screen_switch_s;
    uint16_t info_screen_s;
    uint16_t display_rotation;
    bool display_bgr;
    bool display_invert;
    int16_t display_offset_x;
    int16_t display_offset_y;
    char wifi_ssid[ROBOT_SSID_MAX + 1];
    char wifi_password[ROBOT_WIFI_PASSWORD_MAX + 1];
    char ap_ssid[ROBOT_AP_SSID_MAX + 1];
    char ap_password[ROBOT_WIFI_PASSWORD_MAX + 1];
    char weather_latitude[16];
    char weather_longitude[16];
    char weather_timezone[ROBOT_TIMEZONE_MAX + 1];
    uint16_t weather_interval_min;
} robot_settings_t;

enum {
    SETTINGS_FIELD_SCREEN_MODE      = 1u << 0,
    SETTINGS_FIELD_ANIMATION_ID     = 1u << 1,
    SETTINGS_FIELD_ANIMATION_FPS    = 1u << 2,
    SETTINGS_FIELD_FACE_COLOR       = 1u << 3,
    SETTINGS_FIELD_SCREEN_TIMING    = 1u << 4,
    SETTINGS_FIELD_DISPLAY_ROTATION = 1u << 5,
    SETTINGS_FIELD_DISPLAY_FLAGS    = 1u << 6,
    SETTINGS_FIELD_DISPLAY_OFFSET   = 1u << 7,
    SETTINGS_FIELD_WIFI             = 1u << 8,
    SETTINGS_FIELD_AP               = 1u << 9,
    SETTINGS_FIELD_WEATHER          = 1u << 10,
};

typedef struct {
    uint32_t fields;
    robot_screen_mode_t screen_mode;
    uint8_t animation_id;
    uint8_t animation_fps;
    uint32_t face_color;
    uint16_t screen_switch_s;
    uint16_t info_screen_s;
    uint16_t display_rotation;
    bool display_bgr;
    bool display_invert;
    int16_t display_offset_x;
    int16_t display_offset_y;
    char wifi_ssid[ROBOT_SSID_MAX + 1];
    char wifi_password[ROBOT_WIFI_PASSWORD_MAX + 1];
    char ap_ssid[ROBOT_AP_SSID_MAX + 1];
    char ap_password[ROBOT_WIFI_PASSWORD_MAX + 1];
    char weather_latitude[16];
    char weather_longitude[16];
    char weather_timezone[ROBOT_TIMEZONE_MAX + 1];
    uint16_t weather_interval_min;
} robot_settings_patch_t;

typedef struct {
    bool restart_required;
    bool network_changed;
    bool weather_changed;
} settings_change_t;

void settings_defaults(robot_settings_t *out);
settings_result_t settings_validate(const robot_settings_t *settings);
settings_result_t settings_apply_patch(robot_settings_t *settings,
                                       const robot_settings_patch_t *patch,
                                       settings_change_t *change);

#ifdef ESP_PLATFORM
#include "esp_err.h"
esp_err_t settings_init(void);
void settings_get(robot_settings_t *out);
esp_err_t settings_update(const robot_settings_patch_t *patch, settings_change_t *change);
bool settings_verify_admin(const char *password);
esp_err_t settings_change_admin(const char *old_password, const char *new_password);
#endif
