#include "usb_control.h"
#include "app_controller.h"
#include "robot_settings.h"
#include "sensor.h"
#include "connectivity.h"
#include "animation.h"
#include "driver/usb_serial_jtag.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "cJSON.h"
#include "mbedtls/base64.h"
#include "esp_mac.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#define FRAME_MAX 512
#define GIF_MAX (2 * 1024 * 1024)

static FILE *s_upload;
static unsigned s_upload_slot;
static size_t s_upload_expected, s_upload_received;
static uint32_t s_upload_crc, s_expected_crc;

static uint32_t crc32_chunk(uint32_t crc, const uint8_t *data, size_t length)
{
    for (size_t i = 0; i < length; i++) {
        crc ^= data[i];
        for (int bit = 0; bit < 8; bit++) crc = (crc >> 1) ^ (0xedb88320u & -(crc & 1u));
    }
    return crc;
}

static bool slot_path(unsigned slot, char *path, size_t size)
{
    if (slot >= ANIMATION_COUNT) return false;
    if (slot < 9) snprintf(path, size, "/assets/%s.gif", animation_name(slot));
    else snprintf(path, size, "/assets/user_%02u.gif", slot - 9);
    return true;
}

static void reply(cJSON *out)
{
    char *line = cJSON_PrintUnformatted(out);
    cJSON_Delete(out);
    if (!line) return;
    usb_serial_jtag_write_bytes(line, strlen(line), pdMS_TO_TICKS(1000));
    usb_serial_jtag_write_bytes("\n", 1, pdMS_TO_TICKS(1000));
    free(line);
}

static cJSON *response(const cJSON *request)
{
    cJSON *out = cJSON_CreateObject();
    const cJSON *id = cJSON_GetObjectItemCaseSensitive(request, "id");
    if (cJSON_IsNumber(id) && id->valuedouble == id->valueint && id->valueint >= 0)
        cJSON_AddNumberToObject(out, "id", id->valueint);
    return out;
}

static void error(cJSON *out, const char *message)
{
    cJSON_AddBoolToObject(out, "ok", false);
    cJSON_AddStringToObject(out, "error", message);
}

static void dispatch(const char *line)
{
    cJSON *request = cJSON_Parse(line);
    cJSON *out = response(request);
    const cJSON *op = cJSON_GetObjectItemCaseSensitive(request, "op");
    if (!cJSON_IsString(op)) error(out, "invalid request");
    else if (strcmp(op->valuestring, "hello") == 0) {
        uint8_t mac[6] = {0};
        char chip_id[13];
        esp_efuse_mac_get_default(mac);
        snprintf(chip_id, sizeof(chip_id), "%02x%02x%02x%02x%02x%02x",
                 mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
        cJSON_AddBoolToObject(out, "ok", true);
        cJSON_AddNumberToObject(out, "protocol", 1);
        cJSON_AddStringToObject(out, "device", "desktop-robot");
        cJSON_AddStringToObject(out, "chip_id", chip_id);
    } else if (strcmp(op->valuestring, "get_status") == 0) {
        robot_settings_t s; settings_get(&s);
        sensor_data_t data; sensor_get(&data);
        cJSON_AddBoolToObject(out, "ok", true);
        cJSON_AddNumberToObject(out, "screen", s.screen_mode);
        cJSON_AddNumberToObject(out, "animation", s.animation_id);
        cJSON_AddStringToObject(out, "network", connectivity_status_str(connectivity_status()));
        if (data.valid) {
            cJSON_AddNumberToObject(out, "temperature", data.temperature);
            cJSON_AddNumberToObject(out, "humidity", data.humidity);
        }
    } else if (strcmp(op->valuestring, "get_settings") == 0) {
        robot_settings_t s; settings_get(&s);
        cJSON_AddBoolToObject(out, "ok", true);
        cJSON_AddStringToObject(out, "ssid", s.wifi_ssid);
        cJSON_AddStringToObject(out, "latitude", s.weather_latitude);
        cJSON_AddStringToObject(out, "longitude", s.weather_longitude);
        cJSON_AddStringToObject(out, "timezone", s.weather_timezone);
        cJSON_AddNumberToObject(out, "weather_interval", s.weather_interval_min);
    } else if (strcmp(op->valuestring, "set_wifi") == 0) {
        const cJSON *ssid = cJSON_GetObjectItemCaseSensitive(request, "ssid");
        const cJSON *password = cJSON_GetObjectItemCaseSensitive(request, "password");
        robot_settings_t previous; settings_get(&previous);
        robot_settings_patch_t patch = { .fields = SETTINGS_FIELD_WIFI };
        if (!cJSON_IsString(ssid) || strlen(ssid->valuestring) > ROBOT_SSID_MAX ||
            (password && (!cJSON_IsString(password) || strlen(password->valuestring) > ROBOT_WIFI_PASSWORD_MAX))) {
            error(out, "invalid Wi-Fi settings");
        } else {
            strcpy(patch.wifi_ssid, ssid->valuestring);
            strcpy(patch.wifi_password, password ? password->valuestring : previous.wifi_password);
            settings_change_t change;
            if (settings_update(&patch, &change) != ESP_OK) error(out, "invalid Wi-Fi settings");
            else if (connectivity_apply_settings() != ESP_OK) error(out, "saved, reconnect failed");
            else cJSON_AddBoolToObject(out, "ok", true);
        }
    } else if (strcmp(op->valuestring, "set_weather") == 0) {
        const cJSON *latitude = cJSON_GetObjectItemCaseSensitive(request, "latitude");
        const cJSON *longitude = cJSON_GetObjectItemCaseSensitive(request, "longitude");
        const cJSON *timezone = cJSON_GetObjectItemCaseSensitive(request, "timezone");
        const cJSON *interval = cJSON_GetObjectItemCaseSensitive(request, "interval");
        if (!cJSON_IsString(latitude) || strlen(latitude->valuestring) >= 16 ||
            !cJSON_IsString(longitude) || strlen(longitude->valuestring) >= 16 ||
            !cJSON_IsString(timezone) || strlen(timezone->valuestring) > ROBOT_TIMEZONE_MAX ||
            !cJSON_IsNumber(interval) || interval->valuedouble != interval->valueint ||
            interval->valueint < 5 || interval->valueint > 180) error(out, "invalid weather settings");
        else {
            robot_settings_patch_t patch = { .fields = SETTINGS_FIELD_WEATHER,
                                             .weather_interval_min = interval->valueint };
            strcpy(patch.weather_latitude, latitude->valuestring);
            strcpy(patch.weather_longitude, longitude->valuestring);
            strcpy(patch.weather_timezone, timezone->valuestring);
            settings_change_t change;
            if (settings_update(&patch, &change) != ESP_OK) error(out, "invalid weather settings");
            else cJSON_AddBoolToObject(out, "ok", true);
        }
    } else if (strcmp(op->valuestring, "gif_status") == 0) {
        const cJSON *slot = cJSON_GetObjectItemCaseSensitive(request, "slot");
        char path[48];
        if (!cJSON_IsNumber(slot) || slot->valuedouble != slot->valueint ||
            !slot_path(slot->valueint, path, sizeof(path))) error(out, "invalid GIF slot");
        else {
            struct stat info;
            cJSON_AddBoolToObject(out, "ok", true);
            cJSON_AddStringToObject(out, "name", animation_name(slot->valueint));
            cJSON_AddBoolToObject(out, "present", stat(path, &info) == 0);
            if (stat(path, &info) == 0) cJSON_AddNumberToObject(out, "size", info.st_size);
        }
    } else if (strcmp(op->valuestring, "gif_delete") == 0) {
        const cJSON *slot = cJSON_GetObjectItemCaseSensitive(request, "slot");
        char path[48];
        if (!cJSON_IsNumber(slot) || slot->valuedouble != slot->valueint ||
            !slot_path(slot->valueint, path, sizeof(path))) error(out, "invalid GIF slot");
        else if (remove(path) != 0) error(out, "GIF delete failed");
        else {
            robot_command_t command = { .type = ROBOT_COMMAND_RELOAD_ANIMATION,
                                        .animation_id = slot->valueint };
            if (!app_controller_send(&command, 0)) error(out, "deleted; reload queue full");
            else cJSON_AddBoolToObject(out, "ok", true);
        }
    } else if (strcmp(op->valuestring, "gif_begin") == 0) {
        const cJSON *slot = cJSON_GetObjectItemCaseSensitive(request, "slot");
        const cJSON *size = cJSON_GetObjectItemCaseSensitive(request, "size");
        const cJSON *crc = cJSON_GetObjectItemCaseSensitive(request, "crc32");
        char path[48];
        if (!cJSON_IsNumber(slot) || slot->valuedouble != slot->valueint ||
            !slot_path(slot->valueint, path, sizeof(path)) ||
            !cJSON_IsNumber(size) || size->valuedouble != size->valueint ||
            size->valueint < 14 || size->valueint > GIF_MAX ||
            !cJSON_IsNumber(crc) || crc->valuedouble < 0 || crc->valuedouble > 4294967295.0 ||
            crc->valuedouble != (double)(uint32_t)crc->valuedouble) error(out, "invalid GIF upload");
        else {
            if (s_upload) fclose(s_upload);
            s_upload = fopen("/assets/upload.tmp", "wb");
            s_upload_slot = slot->valueint;
            s_upload_expected = size->valueint;
            s_upload_received = 0;
            s_upload_crc = 0xffffffffu;
            s_expected_crc = (uint32_t)crc->valuedouble;
            if (!s_upload) error(out, "GIF storage unavailable");
            else cJSON_AddBoolToObject(out, "ok", true);
        }
    } else if (strcmp(op->valuestring, "gif_chunk") == 0) {
        const cJSON *data = cJSON_GetObjectItemCaseSensitive(request, "data");
        uint8_t decoded[192]; size_t length = 0;
        if (!s_upload || !cJSON_IsString(data) || strlen(data->valuestring) > 256 ||
            mbedtls_base64_decode(decoded, sizeof(decoded), &length,
                (const unsigned char *)data->valuestring, strlen(data->valuestring)) != 0 ||
            s_upload_received + length > s_upload_expected ||
            fwrite(decoded, 1, length, s_upload) != length) error(out, "GIF write failed");
        else { s_upload_received += length; s_upload_crc = crc32_chunk(s_upload_crc, decoded, length);
               cJSON_AddBoolToObject(out, "ok", true); }
    } else if (strcmp(op->valuestring, "gif_finish") == 0) {
        char path[48]; slot_path(s_upload_slot, path, sizeof(path));
        if (!s_upload) error(out, "no GIF upload");
        else if (s_upload_received != s_upload_expected ||
                 (s_upload_crc ^ 0xffffffffu) != s_expected_crc) {
            fclose(s_upload); s_upload = NULL; remove("/assets/upload.tmp");
            error(out, "GIF size or checksum mismatch");
        }
        else {
            bool saved = fflush(s_upload) == 0;
            if (fclose(s_upload) != 0) saved = false;
            s_upload = NULL;
            FILE *check = saved ? fopen("/assets/upload.tmp", "rb") : NULL;
            char header[6];
            bool valid = check && fread(header, 1, sizeof(header), check) == sizeof(header) &&
                         (!memcmp(header, "GIF87a", 6) || !memcmp(header, "GIF89a", 6));
            if (check) fclose(check);
            if (!valid) error(out, "invalid GIF file");
            else {
                char backup[52]; snprintf(backup, sizeof(backup), "%s.bak", path);
                struct stat old_info, backup_info;
                bool had_old = stat(path, &old_info) == 0;
                if (stat(backup, &backup_info) == 0 || (had_old && rename(path, backup) != 0)) {
                    error(out, "GIF backup failed");
                } else if (rename("/assets/upload.tmp", path) != 0) {
                    if (had_old) rename(backup, path);
                    error(out, "GIF replace failed");
                } else {
                    if (had_old) remove(backup);
                    robot_command_t command = { .type = ROBOT_COMMAND_RELOAD_ANIMATION,
                                                .animation_id = s_upload_slot };
                    if (!app_controller_send(&command, 0)) error(out, "saved; reload queue full");
                    else cJSON_AddBoolToObject(out, "ok", true);
                }
            }
        }
    } else if (strcmp(op->valuestring, "show_screen") == 0 || strcmp(op->valuestring, "show_animation") == 0) {
        bool animation = strcmp(op->valuestring, "show_animation") == 0;
        const cJSON *value = cJSON_GetObjectItemCaseSensitive(request, animation ? "animation" : "screen");
        robot_command_t command = { .type = animation ? ROBOT_COMMAND_PLAY_ANIMATION : ROBOT_COMMAND_SET_SCREEN };
        if (!cJSON_IsNumber(value) || value->valuedouble != value->valueint ||
            value->valueint < (animation ? 0 : ROBOT_SCREEN_FACE) ||
            value->valueint > (animation ? ANIMATION_COUNT - 1 : ROBOT_SCREEN_SENSOR)) error(out, "invalid selection");
        else {
            if (animation) command.animation_id = value->valueint;
            else command.screen = value->valueint;
            if (!robot_command_validate(&command)) error(out, "invalid selection");
            else if (!app_controller_send(&command, 0)) error(out, "command queue full");
            else cJSON_AddBoolToObject(out, "ok", true);
        }
    } else error(out, "unknown operation");
    reply(out);
    cJSON_Delete(request);
}

static void control_task(void *arg)
{
    char frame[FRAME_MAX + 1];
    size_t length = 0;
    bool overflow = false;
    uint8_t byte;
    for (;;) {
        if (usb_serial_jtag_read_bytes(&byte, 1, pdMS_TO_TICKS(100)) != 1) continue;
        if (byte == '\n') {
            if (overflow) {
                cJSON *out = cJSON_CreateObject(); error(out, "frame too long"); reply(out);
            } else if (length) { frame[length] = '\0'; dispatch(frame); }
            length = 0; overflow = false;
        } else if (byte != '\r' && !overflow) {
            if (length < FRAME_MAX) frame[length++] = byte;
            else overflow = true;
        }
    }
}

esp_err_t usb_control_start(void)
{
    usb_serial_jtag_driver_config_t cfg = {
        .rx_buffer_size = 1024, .tx_buffer_size = 1024,
    };
    esp_err_t err = usb_serial_jtag_driver_install(&cfg);
    if (err != ESP_OK) return err;
    return xTaskCreate(control_task, "usb_control", 4096, NULL, 3, NULL) == pdPASS
        ? ESP_OK : ESP_ERR_NO_MEM;
}
