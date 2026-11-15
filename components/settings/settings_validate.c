#include "robot_settings.h"
#include <string.h>
#include <stdlib.h>

static size_t bounded_len(const char *value, size_t capacity)
{
    size_t n = 0;
    while (n < capacity && value[n]) n++;
    return n;
}

static bool ascii_password(const char *value, size_t min_len)
{
    size_t n = bounded_len(value, ROBOT_WIFI_PASSWORD_MAX + 1);
    if (n < min_len || n > ROBOT_WIFI_PASSWORD_MAX) return false;
    for (size_t i = 0; i < n; i++)
        if ((unsigned char)value[i] < 0x20 || (unsigned char)value[i] > 0x7e) return false;
    return true;
}

static bool rotation_valid(uint16_t rotation)
{
    return rotation == 0 || rotation == 90 || rotation == 180 || rotation == 270;
}

static bool terminated(const char *value, size_t capacity) { return bounded_len(value, capacity) < capacity; }

static bool coordinate(const char *value, float low, float high)
{
    char *end;
    float parsed = strtof(value, &end);
    return value[0] && *end == '\0' && parsed >= low && parsed <= high;
}

void settings_defaults(robot_settings_t *s)
{
    if (!s) return;
    *s = (robot_settings_t){
        .screen_mode = ROBOT_SCREEN_FACE,
        .animation_fps = 25,
        .face_color = 0x00d9ff,
        .screen_switch_s = 30,
        .info_screen_s = 6,
        .display_rotation = 0,
        .display_invert = true,
        .weather_interval_min = 15,
    };
    strcpy(s->ap_ssid, "DesktopRobot");
    strcpy(s->ap_password, "change-me");
    strcpy(s->weather_latitude, "10.8231");
    strcpy(s->weather_longitude, "106.6297");
    strcpy(s->weather_timezone, "auto");
}

settings_result_t settings_validate(const robot_settings_t *s)
{
    if (!s) return SETTINGS_ERR_ARGUMENT;
    if ((unsigned)s->screen_mode > ROBOT_SCREEN_SENSOR || s->animation_id >= 41 ||
        s->animation_fps < 5 || s->animation_fps > 50 || s->face_color > 0xffffff ||
        s->screen_switch_s == 0 || s->screen_switch_s > 3600 ||
        s->info_screen_s == 0 || s->info_screen_s > 3600)
        return SETTINGS_ERR_ANIMATION;
    if (!rotation_valid(s->display_rotation) || s->display_offset_x < -320 || s->display_offset_x > 320 ||
        s->display_offset_y < -320 || s->display_offset_y > 320)
        return SETTINGS_ERR_DISPLAY;
    if (bounded_len(s->wifi_ssid, sizeof(s->wifi_ssid)) > ROBOT_SSID_MAX ||
        bounded_len(s->wifi_password, sizeof(s->wifi_password)) > ROBOT_WIFI_PASSWORD_MAX ||
        !terminated(s->wifi_ssid, sizeof(s->wifi_ssid)) || !terminated(s->wifi_password, sizeof(s->wifi_password)) ||
        (s->wifi_password[0] && !ascii_password(s->wifi_password, 8)))
        return SETTINGS_ERR_WIFI;
    if (!s->ap_ssid[0] || bounded_len(s->ap_ssid, sizeof(s->ap_ssid)) > ROBOT_AP_SSID_MAX ||
        (s->ap_password[0] && !ascii_password(s->ap_password, 8)))
        return SETTINGS_ERR_AP_PASSWORD;
    if (!terminated(s->weather_latitude, sizeof(s->weather_latitude)) ||
        !terminated(s->weather_longitude, sizeof(s->weather_longitude)) ||
        !terminated(s->weather_timezone, sizeof(s->weather_timezone)) ||
        !coordinate(s->weather_latitude, -90, 90) || !coordinate(s->weather_longitude, -180, 180) ||
        !s->weather_timezone[0] ||
        s->weather_interval_min < 5 || s->weather_interval_min > 180)
        return SETTINGS_ERR_WEATHER;
    return SETTINGS_OK;
}

settings_result_t settings_apply_patch(robot_settings_t *s, const robot_settings_patch_t *p,
                                       settings_change_t *change)
{
    if (!s || !p || !change) return SETTINGS_ERR_ARGUMENT;
    robot_settings_t next = *s;
    *change = (settings_change_t){0};
    if (p->fields & SETTINGS_FIELD_SCREEN_MODE) next.screen_mode = p->screen_mode;
    if (p->fields & SETTINGS_FIELD_ANIMATION_ID) next.animation_id = p->animation_id;
    if (p->fields & SETTINGS_FIELD_ANIMATION_FPS) next.animation_fps = p->animation_fps;
    if (p->fields & SETTINGS_FIELD_FACE_COLOR) next.face_color = p->face_color;
    if (p->fields & SETTINGS_FIELD_SCREEN_TIMING) {
        next.screen_switch_s = p->screen_switch_s;
        next.info_screen_s = p->info_screen_s;
    }
    if (p->fields & SETTINGS_FIELD_DISPLAY_ROTATION) {
        next.display_rotation = p->display_rotation;
        change->restart_required = true;
    }
    if (p->fields & SETTINGS_FIELD_DISPLAY_FLAGS) {
        next.display_bgr = p->display_bgr;
        next.display_invert = p->display_invert;
        change->restart_required = true;
    }
    if (p->fields & SETTINGS_FIELD_DISPLAY_OFFSET) {
        next.display_offset_x = p->display_offset_x;
        next.display_offset_y = p->display_offset_y;
        change->restart_required = true;
    }
    if (p->fields & SETTINGS_FIELD_WIFI) {
        memcpy(next.wifi_ssid, p->wifi_ssid, sizeof(next.wifi_ssid));
        memcpy(next.wifi_password, p->wifi_password, sizeof(next.wifi_password));
        change->network_changed = true;
    }
    if (p->fields & SETTINGS_FIELD_AP) {
        memcpy(next.ap_ssid, p->ap_ssid, sizeof(next.ap_ssid));
        memcpy(next.ap_password, p->ap_password, sizeof(next.ap_password));
        change->network_changed = true;
    }
    if (p->fields & SETTINGS_FIELD_WEATHER) {
        memcpy(next.weather_latitude, p->weather_latitude, sizeof(next.weather_latitude));
        memcpy(next.weather_longitude, p->weather_longitude, sizeof(next.weather_longitude));
        memcpy(next.weather_timezone, p->weather_timezone, sizeof(next.weather_timezone));
        next.weather_interval_min = p->weather_interval_min;
        change->weather_changed = true;
    }
    settings_result_t result = settings_validate(&next);
    if (result == SETTINGS_OK) *s = next;
    else *change = (settings_change_t){0};
    return result;
}
