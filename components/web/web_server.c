#include "web_server.h"
#include "web_auth.h"
#include "robot_settings.h"
#include "connectivity.h"
#include "sensor.h"
#include "weather.h"
#include "app_controller.h"
#include "esp_https_server.h"
#include "esp_timer.h"
#include "esp_random.h"
#include "cJSON.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

extern const unsigned char index_html_start[] asm("_binary_index_html_start");
extern const unsigned char index_html_end[] asm("_binary_index_html_end");
extern const unsigned char app_css_start[] asm("_binary_app_css_start");
extern const unsigned char app_css_end[] asm("_binary_app_css_end");
extern const unsigned char app_js_start[] asm("_binary_app_js_start");
extern const unsigned char app_js_end[] asm("_binary_app_js_end");
extern const unsigned char cert_start[] asm("_binary_server_crt_start");
extern const unsigned char cert_end[] asm("_binary_server_crt_end");
extern const unsigned char key_start[] asm("_binary_server_key_start");
extern const unsigned char key_end[] asm("_binary_server_key_end");

static web_auth_t s_auth;
static httpd_handle_t s_server;
static uint32_t now_ms(void) { return (uint32_t)(esp_timer_get_time() / 1000); }

static esp_err_t json_reply(httpd_req_t *req, const char *status, cJSON *json)
{
    char *text = cJSON_PrintUnformatted(json);
    cJSON_Delete(json);
    if (!text) return ESP_ERR_NO_MEM;
    httpd_resp_set_status(req, status); httpd_resp_set_type(req, "application/json");
    esp_err_t err = httpd_resp_send(req, text, HTTPD_RESP_USE_STRLEN);
    free(text); return err;
}

static esp_err_t error_reply(httpd_req_t *req, const char *status, const char *message)
{
    cJSON *j = cJSON_CreateObject(); cJSON_AddStringToObject(j, "error", message); return json_reply(req, status, j);
}

static cJSON *read_json(httpd_req_t *req)
{
    if (req->content_len <= 0 || req->content_len > 1024) return NULL;
    char body[1025]; int total = 0;
    while (total < req->content_len) {
        int got = httpd_req_recv(req, body + total, req->content_len - total);
        if (got <= 0) return NULL;
        total += got;
    }
    body[total] = '\0'; return cJSON_ParseWithLength(body, total);
}

static bool cookie_token(httpd_req_t *req, char token[33])
{
    char cookie[160]; size_t len = httpd_req_get_hdr_value_len(req, "Cookie");
    if (!len || len >= sizeof(cookie) || httpd_req_get_hdr_value_str(req, "Cookie", cookie, sizeof(cookie)) != ESP_OK) return false;
    const char *p = strstr(cookie, "robot_session="); if (!p) return false; p += 14;
    size_t n = strcspn(p, "; "); if (n != 32) return false;
    memcpy(token, p, n); token[n] = '\0'; return true;
}

static bool authorized(httpd_req_t *req)
{
    char token[33], csrf[32];
    if (!cookie_token(req, token)) return false;
    if (httpd_req_get_hdr_value_str(req, "X-CSRF-Token", csrf, sizeof(csrf)) != ESP_OK) return false;
    return web_auth_authorize(&s_auth, token, csrf, now_ms());
}

static esp_err_t asset(httpd_req_t *req, const char *type, const unsigned char *start, const unsigned char *end)
{
    httpd_resp_set_type(req, type); httpd_resp_set_hdr(req, "Cache-Control", "no-store");
    size_t len = (size_t)(end - start);
    if (len && start[len - 1] == '\0') len--; // EMBED_TXTFILES includes a terminator.
    return httpd_resp_send(req, (const char *)start, len);
}
static esp_err_t index_get(httpd_req_t *r) { return asset(r, "text/html", index_html_start, index_html_end); }
static esp_err_t css_get(httpd_req_t *r) { return asset(r, "text/css", app_css_start, app_css_end); }
static esp_err_t js_get(httpd_req_t *r) { return asset(r, "application/javascript", app_js_start, app_js_end); }

static void random_hex(char *out, size_t bytes)
{
    uint8_t raw[16]; esp_fill_random(raw, bytes);
    static const char d[] = "0123456789abcdef";
    for (size_t i = 0; i < bytes; i++) { out[i * 2] = d[raw[i] >> 4]; out[i * 2 + 1] = d[raw[i] & 15]; }
    out[bytes * 2] = '\0';
}

static esp_err_t login_post(httpd_req_t *req)
{
    uint32_t now = now_ms();
    if (!web_auth_login_allowed(&s_auth, now)) return error_reply(req, "429 Too Many Requests", "Thử lại sau");
    cJSON *j = read_json(req); cJSON *p = j ? cJSON_GetObjectItemCaseSensitive(j, "password") : NULL;
    bool ok = cJSON_IsString(p) && settings_verify_admin(p->valuestring);
    if (j) cJSON_Delete(j);
    if (!ok) { web_auth_login_failed(&s_auth, now); return error_reply(req, "401 Unauthorized", "Sai mật khẩu"); }
    web_auth_login_succeeded(&s_auth);
    web_session_t *s = web_auth_create(&s_auth, now, esp_random());
    random_hex(s->token, 16); random_hex(s->csrf, 8);
    char cookie[128]; snprintf(cookie, sizeof(cookie), "robot_session=%s; Path=/; Max-Age=1800; Secure; HttpOnly; SameSite=Strict", s->token);
    httpd_resp_set_hdr(req, "Set-Cookie", cookie);
    cJSON *out = cJSON_CreateObject(); cJSON_AddStringToObject(out, "csrf", s->csrf); return json_reply(req, "200 OK", out);
}

static esp_err_t status_get(httpd_req_t *req)
{
    char token[33]; if (!cookie_token(req, token) || !web_auth_find(&s_auth, token, now_ms())) return error_reply(req, "401 Unauthorized", "Cần đăng nhập");
    connectivity_info_t net; connectivity_get_info(&net);
    sensor_data_t sensor; sensor_get(&sensor);
    cJSON *j = cJSON_CreateObject();
    cJSON_AddStringToObject(j, "network", connectivity_status_str(net.state.sta));
    if (sensor.valid) { cJSON_AddNumberToObject(j, "temperature", sensor.temperature); cJSON_AddNumberToObject(j, "humidity", sensor.humidity); }
    else { cJSON_AddNullToObject(j, "temperature"); cJSON_AddNullToObject(j, "humidity"); }
    if (net.state.last_error) { char e[24]; snprintf(e, sizeof(e), "Wi-Fi: %ld", (long)net.state.last_error); cJSON_AddStringToObject(j, "last_error", e); }
    else cJSON_AddStringToObject(j, "last_error", "");
    cJSON_AddBoolToObject(j, "brightness_supported", CONFIG_ROBOT_PIN_LCD_BL >= 0);
    return json_reply(req, "200 OK", j);
}

static esp_err_t settings_get_handler(httpd_req_t *req)
{
    char token[33]; if (!cookie_token(req, token) || !web_auth_find(&s_auth, token, now_ms())) return error_reply(req, "401 Unauthorized", "Cần đăng nhập");
    robot_settings_t s; settings_get(&s);
    cJSON *j = cJSON_CreateObject();
    cJSON_AddNumberToObject(j, "screen", s.screen_mode); cJSON_AddNumberToObject(j, "animation", s.animation_id);
    cJSON_AddNumberToObject(j, "fps", s.animation_fps); cJSON_AddNumberToObject(j, "face_color", s.face_color);
    cJSON_AddNumberToObject(j, "screen_switch", s.screen_switch_s); cJSON_AddNumberToObject(j, "info_duration", s.info_screen_s);
    cJSON_AddNumberToObject(j, "rotation", s.display_rotation); cJSON_AddBoolToObject(j, "bgr", s.display_bgr);
    cJSON_AddBoolToObject(j, "invert", s.display_invert); cJSON_AddNumberToObject(j, "offset_x", s.display_offset_x); cJSON_AddNumberToObject(j, "offset_y", s.display_offset_y);
    cJSON_AddStringToObject(j, "ssid", s.wifi_ssid); cJSON_AddStringToObject(j, "ap_ssid", s.ap_ssid);
    cJSON_AddStringToObject(j, "latitude", s.weather_latitude); cJSON_AddStringToObject(j, "longitude", s.weather_longitude);
    cJSON_AddStringToObject(j, "timezone", s.weather_timezone); cJSON_AddNumberToObject(j, "weather_interval", s.weather_interval_min);
    return json_reply(req, "200 OK", j);
}

static esp_err_t command_post(httpd_req_t *req)
{
    if (!authorized(req)) return error_reply(req, "403 Forbidden", "Phiên hoặc CSRF không hợp lệ");
    cJSON *j = read_json(req); if (!j) return error_reply(req, "400 Bad Request", "JSON không hợp lệ");
    robot_command_t c = {0}; cJSON *screen = cJSON_GetObjectItemCaseSensitive(j, "screen"); cJSON *animation = cJSON_GetObjectItemCaseSensitive(j, "animation");
    if (cJSON_IsNumber(screen) && screen->valuedouble == screen->valueint && screen->valueint >= ROBOT_SCREEN_FACE && screen->valueint <= ROBOT_SCREEN_SENSOR) { c.type = ROBOT_COMMAND_SET_SCREEN; c.screen = screen->valueint; }
    else if (cJSON_IsNumber(animation) && animation->valuedouble == animation->valueint && animation->valueint >= 0 && animation->valueint < 9) { c.type = ROBOT_COMMAND_PLAY_ANIMATION; c.animation_id = animation->valueint; }
    else { cJSON_Delete(j); return error_reply(req, "400 Bad Request", "Thiếu lệnh"); }
    cJSON_Delete(j);
    if (!app_controller_send(&c, 0)) return error_reply(req, "503 Service Unavailable", "Hàng đợi điều khiển đang đầy");
    cJSON *out = cJSON_CreateObject(); cJSON_AddBoolToObject(out, "ok", true); return json_reply(req, "200 OK", out);
}

static bool copy_json_string(cJSON *j, const char *key, char *out, size_t size)
{
    cJSON *v = cJSON_GetObjectItemCaseSensitive(j, key);
    if (!cJSON_IsString(v) || strlen(v->valuestring) >= size) return false;
    strlcpy(out, v->valuestring, size); return true;
}

static esp_err_t settings_post(httpd_req_t *req)
{
    if (!authorized(req)) return error_reply(req, "403 Forbidden", "Phiên hoặc CSRF không hợp lệ");
    cJSON *j = read_json(req); if (!j) return error_reply(req, "400 Bad Request", "JSON không hợp lệ");
    robot_settings_t current; settings_get(&current);
    robot_settings_patch_t p = {0}; cJSON *v;
    if (cJSON_IsNumber(v = cJSON_GetObjectItemCaseSensitive(j, "fps"))) { if (v->valuedouble != v->valueint || v->valueint < 5 || v->valueint > 50) goto invalid; p.fields |= SETTINGS_FIELD_ANIMATION_FPS; p.animation_fps = v->valueint; }
    if (cJSON_IsNumber(v = cJSON_GetObjectItemCaseSensitive(j, "face_color"))) { if (v->valuedouble != v->valueint || v->valueint < 0 || v->valuedouble > 0xffffff) goto invalid; p.fields |= SETTINGS_FIELD_FACE_COLOR; p.face_color = (uint32_t)v->valuedouble; }
    if (cJSON_IsNumber(v = cJSON_GetObjectItemCaseSensitive(j, "screen_switch"))) { p.fields |= SETTINGS_FIELD_SCREEN_TIMING; p.screen_switch_s = v->valueint; cJSON *d = cJSON_GetObjectItemCaseSensitive(j, "info_duration"); p.info_screen_s = cJSON_IsNumber(d) ? d->valueint : current.info_screen_s; }
    if (cJSON_IsNumber(v = cJSON_GetObjectItemCaseSensitive(j, "rotation"))) { if (v->valuedouble != v->valueint || (v->valueint != 0 && v->valueint != 90 && v->valueint != 180 && v->valueint != 270)) goto invalid; p.fields |= SETTINGS_FIELD_DISPLAY_ROTATION; p.display_rotation = v->valueint; }
    if (cJSON_HasObjectItem(j, "bgr")) { p.fields |= SETTINGS_FIELD_DISPLAY_FLAGS; p.display_bgr = cJSON_IsTrue(cJSON_GetObjectItemCaseSensitive(j, "bgr")); p.display_invert = cJSON_IsTrue(cJSON_GetObjectItemCaseSensitive(j, "invert")); }
    if (cJSON_IsNumber(v = cJSON_GetObjectItemCaseSensitive(j, "offset_x"))) { p.fields |= SETTINGS_FIELD_DISPLAY_OFFSET; p.display_offset_x = v->valueint; cJSON *y = cJSON_GetObjectItemCaseSensitive(j, "offset_y"); p.display_offset_y = cJSON_IsNumber(y) ? y->valueint : current.display_offset_y; }
    if (cJSON_HasObjectItem(j, "ssid")) { p.fields |= SETTINGS_FIELD_WIFI; if (!copy_json_string(j, "ssid", p.wifi_ssid, sizeof(p.wifi_ssid))) goto invalid; cJSON *pw = cJSON_GetObjectItemCaseSensitive(j, "wifi_password"); if (cJSON_IsString(pw) && pw->valuestring[0]) { if (!copy_json_string(j, "wifi_password", p.wifi_password, sizeof(p.wifi_password))) goto invalid; } else strlcpy(p.wifi_password, current.wifi_password, sizeof(p.wifi_password)); cJSON *open = cJSON_GetObjectItemCaseSensitive(j, "open_network"); if (cJSON_IsTrue(open)) p.wifi_password[0] = '\0'; }
    cJSON *ap_password = cJSON_GetObjectItemCaseSensitive(j, "ap_password");
    if (cJSON_IsString(ap_password) && ap_password->valuestring[0]) { p.fields |= SETTINGS_FIELD_AP; strlcpy(p.ap_ssid, current.ap_ssid, sizeof(p.ap_ssid)); if (!copy_json_string(j, "ap_password", p.ap_password, sizeof(p.ap_password))) goto invalid; }
    if (cJSON_HasObjectItem(j, "latitude")) { p.fields |= SETTINGS_FIELD_WEATHER; if (!copy_json_string(j, "latitude", p.weather_latitude, sizeof(p.weather_latitude)) || !copy_json_string(j, "longitude", p.weather_longitude, sizeof(p.weather_longitude)) || !copy_json_string(j, "timezone", p.weather_timezone, sizeof(p.weather_timezone))) goto invalid; cJSON *interval = cJSON_GetObjectItemCaseSensitive(j, "weather_interval"); if (!cJSON_IsNumber(interval) || interval->valuedouble != interval->valueint || interval->valueint < 5 || interval->valueint > 180) goto invalid; p.weather_interval_min = interval->valueint; }
    cJSON_Delete(j);
    settings_change_t preview; robot_settings_t next = current;
    if (settings_apply_patch(&next, &p, &preview) != SETTINGS_OK) return error_reply(req, "400 Bad Request", "Giá trị cài đặt không hợp lệ");
    settings_change_t applied;
    if (settings_update(&p, &applied) != ESP_OK) return error_reply(req, "500 Internal Server Error", "Không lưu được cài đặt");
    if (applied.network_changed && connectivity_apply_settings() != ESP_OK) return error_reply(req, "500 Internal Server Error", "Đã lưu nhưng chưa kết nối lại được");
    robot_command_t c = { .type = ROBOT_COMMAND_APPLY_SETTINGS, .patch = p };
    bool runtime = app_controller_send(&c, 0);
    cJSON *out = cJSON_CreateObject(); cJSON_AddBoolToObject(out, "ok", true); cJSON_AddBoolToObject(out, "runtime_queued", runtime); cJSON_AddBoolToObject(out, "restart_required", preview.restart_required); return json_reply(req, runtime ? "200 OK" : "202 Accepted", out);
invalid:
    cJSON_Delete(j); return error_reply(req, "400 Bad Request", "Giá trị cài đặt không hợp lệ");
}

static esp_err_t password_post(httpd_req_t *req)
{
    if (!authorized(req)) return error_reply(req, "403 Forbidden", "Phiên hoặc CSRF không hợp lệ");
    cJSON *j = read_json(req); cJSON *old = j ? cJSON_GetObjectItemCaseSensitive(j, "old_password") : NULL; cJSON *newp = j ? cJSON_GetObjectItemCaseSensitive(j, "new_password") : NULL;
    esp_err_t err = cJSON_IsString(old) && cJSON_IsString(newp) ? settings_change_admin(old->valuestring, newp->valuestring) : ESP_ERR_INVALID_ARG;
    if (j) cJSON_Delete(j);
    if (err != ESP_OK) return error_reply(req, "400 Bad Request", "Không đổi được mật khẩu");
    web_auth_invalidate_all(&s_auth); cJSON *out = cJSON_CreateObject(); cJSON_AddBoolToObject(out, "ok", true); return json_reply(req, "200 OK", out);
}

esp_err_t web_server_start(void)
{
    web_auth_init(&s_auth);
    httpd_ssl_config_t cfg = HTTPD_SSL_CONFIG_DEFAULT();
    cfg.httpd.max_uri_handlers = 10;
    cfg.servercert = cert_start; cfg.servercert_len = cert_end - cert_start;
    cfg.prvtkey_pem = key_start; cfg.prvtkey_len = key_end - key_start;
    esp_err_t err = httpd_ssl_start(&s_server, &cfg); if (err != ESP_OK) return err;
    const httpd_uri_t routes[] = {
        { .uri="/", .method=HTTP_GET, .handler=index_get }, { .uri="/app.css", .method=HTTP_GET, .handler=css_get },
        { .uri="/app.js", .method=HTTP_GET, .handler=js_get }, { .uri="/api/login", .method=HTTP_POST, .handler=login_post },
        { .uri="/api/status", .method=HTTP_GET, .handler=status_get }, { .uri="/api/command", .method=HTTP_POST, .handler=command_post },
        { .uri="/api/settings", .method=HTTP_GET, .handler=settings_get_handler },
        { .uri="/api/settings", .method=HTTP_POST, .handler=settings_post }, { .uri="/api/password", .method=HTTP_POST, .handler=password_post },
    };
    for (size_t i = 0; i < sizeof(routes) / sizeof(routes[0]); i++) httpd_register_uri_handler(s_server, &routes[i]);
    return ESP_OK;
}
