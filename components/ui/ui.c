#include <stdio.h>
#include <string.h>
#include "ui.h"
#include "esp_timer.h"

#define COL_BG      0x000000
#define COL_PANEL   0x0B1016
#define COL_TEXT    0xE8EEF2
#define COL_DIM     0x7A8792
#define COL_WARN    0xFFA040
#define COL_TEMP    0xFF9F43
#define COL_HUM     0x2ED8FF

static lv_obj_t *s_scr[3];
static ui_screen_t s_cur = UI_SCREEN_FACE;

// weather screen
static lv_obj_t *s_w_wifi, *s_w_temp, *s_w_cond, *s_w_detail, *s_w_updated;
static lv_obj_t *s_sun, *s_moon_cut, *s_cloud[3], *s_drop[3], *s_fog[3];
static int s_icon_key = -1;
// sensor screen
static lv_obj_t *s_s_temp, *s_s_hum, *s_s_status;

/* Only touch the label (and invalidate its area) when the text really changes. */
static void set_text(lv_obj_t *l, const char *txt)
{
    if (strcmp(lv_label_get_text(l), txt) != 0) lv_label_set_text(l, txt);
}

static void set_color(lv_obj_t *l, uint32_t hex)
{
    lv_color_t c = lv_color_hex(hex);
    if (!lv_color_eq(lv_obj_get_style_text_color(l, LV_PART_MAIN), c))
        lv_obj_set_style_text_color(l, c, 0);
}

static lv_obj_t *make_screen(uint32_t bg)
{
    lv_obj_t *s = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(s, lv_color_hex(bg), 0);
    lv_obj_set_style_bg_opa(s, LV_OPA_COVER, 0);
    lv_obj_remove_flag(s, LV_OBJ_FLAG_SCROLLABLE);
    return s;
}

static lv_obj_t *make_label(lv_obj_t *p, const lv_font_t *f, uint32_t col, lv_align_t al, int x, int y, const char *txt)
{
    lv_obj_t *l = lv_label_create(p);
    lv_obj_set_style_text_font(l, f, 0);
    lv_obj_set_style_text_color(l, lv_color_hex(col), 0);
    lv_label_set_text(l, txt);
    lv_obj_align(l, al, x, y);
    return l;
}

static lv_obj_t *blob(lv_obj_t *p, int w, int h, int r, uint32_t col, int x, int y)
{
    lv_obj_t *o = lv_obj_create(p);
    lv_obj_remove_style_all(o);
    lv_obj_set_size(o, w, h);
    lv_obj_set_style_radius(o, r, 0);
    lv_obj_set_style_bg_opa(o, LV_OPA_COVER, 0);
    lv_obj_set_style_bg_color(o, lv_color_hex(col), 0);
    lv_obj_align(o, LV_ALIGN_CENTER, x, y);
    return o;
}

/* Weather icon drawn from a handful of shapes (created once, shown/hidden). */
static void icon_create(lv_obj_t *p)
{
    lv_obj_t *box = lv_obj_create(p);
    lv_obj_remove_style_all(box);
    lv_obj_set_size(box, 84, 84);
    lv_obj_align(box, LV_ALIGN_TOP_MID, 0, 26);

    s_sun = blob(box, 44, 44, LV_RADIUS_CIRCLE, 0xFFC800, 0, 0);
    s_moon_cut = blob(box, 36, 36, LV_RADIUS_CIRCLE, COL_PANEL, 12, -8);
    s_cloud[0] = blob(box, 30, 30, LV_RADIUS_CIRCLE, 0xC8D2DC, -10, -2);
    s_cloud[1] = blob(box, 36, 36, LV_RADIUS_CIRCLE, 0xC8D2DC, 8, -6);
    s_cloud[2] = blob(box, 62, 22, 11, 0xC8D2DC, 0, 10);
    for (int i = 0; i < 3; i++) {
        s_drop[i] = blob(box, 4, 12, 2, 0x4A90E2, -16 + 16 * i, 34);
        s_fog[i] = blob(box, 58, 5, 2, 0x9AA5AE, 0, -12 + 12 * i);
    }
}

static void show(lv_obj_t *o, bool on)
{
    if (on) lv_obj_remove_flag(o, LV_OBJ_FLAG_HIDDEN);
    else lv_obj_add_flag(o, LV_OBJ_FLAG_HIDDEN);
}

static void icon_set(wx_category_t cat, bool is_day)
{
    int key = cat * 2 + is_day;
    if (key == s_icon_key) return;
    s_icon_key = key;

    bool sky = cat == WX_CLEAR || cat == WX_PARTLY_CLOUDY;
    bool cloud = !sky || cat == WX_PARTLY_CLOUDY;
    cloud = cloud && cat != WX_FOG;
    bool drops = cat == WX_DRIZZLE || cat == WX_RAIN || cat == WX_SNOW || cat == WX_THUNDER;

    show(s_sun, sky);
    lv_obj_set_style_bg_color(s_sun, lv_color_hex(is_day ? 0xFFC800 : 0xE8E4C8), 0);
    lv_obj_align(s_sun, LV_ALIGN_CENTER, cat == WX_PARTLY_CLOUDY ? -14 : 0, cat == WX_PARTLY_CLOUDY ? -14 : 0);
    show(s_moon_cut, sky && !is_day && cat == WX_CLEAR);
    for (int i = 0; i < 3; i++) {
        show(s_cloud[i], cloud);
        show(s_fog[i], cat == WX_FOG);
        show(s_drop[i], drops);
        bool snow = cat == WX_SNOW;
        lv_obj_set_size(s_drop[i], snow ? 7 : 4, snow ? 7 : (cat == WX_DRIZZLE ? 7 : 12));
        lv_obj_set_style_radius(s_drop[i], snow ? LV_RADIUS_CIRCLE : 2, 0);
        lv_obj_set_style_bg_color(s_drop[i], lv_color_hex(snow ? 0xFFFFFF : cat == WX_THUNDER ? 0xFFD000 : 0x4A90E2), 0);
    }
}

esp_err_t ui_init(void)
{
    s_scr[UI_SCREEN_FACE] = make_screen(COL_BG);

    lv_obj_t *w = s_scr[UI_SCREEN_WEATHER] = make_screen(COL_PANEL);
    s_w_wifi = make_label(w, &lv_font_montserrat_14, COL_DIM, LV_ALIGN_TOP_LEFT, 10, 6, LV_SYMBOL_WIFI " ...");
    icon_create(w);
    s_w_temp = make_label(w, &lv_font_montserrat_40, COL_TEXT, LV_ALIGN_TOP_MID, 0, 112, "--\xC2\xB0" "C");
    s_w_cond = make_label(w, &lv_font_montserrat_20, COL_TEXT, LV_ALIGN_TOP_MID, 0, 160, "Waiting for weather");
    s_w_detail = make_label(w, &lv_font_montserrat_14, COL_DIM, LV_ALIGN_TOP_MID, 0, 190, "");
    s_w_updated = make_label(w, &lv_font_montserrat_14, COL_DIM, LV_ALIGN_BOTTOM_MID, 0, -6, "");
    icon_set(WX_UNKNOWN, true);

    lv_obj_t *s = s_scr[UI_SCREEN_SENSOR] = make_screen(COL_PANEL);
    make_label(s, &lv_font_montserrat_14, COL_DIM, LV_ALIGN_TOP_MID, 0, 10, "INDOOR");
    s_s_temp = make_label(s, &lv_font_montserrat_40, COL_TEMP, LV_ALIGN_TOP_MID, 0, 42, "--\xC2\xB0" "C");
    make_label(s, &lv_font_montserrat_14, COL_DIM, LV_ALIGN_TOP_MID, 0, 90, "Temperature");
    s_s_hum = make_label(s, &lv_font_montserrat_40, COL_HUM, LV_ALIGN_TOP_MID, 0, 120, "--%");
    make_label(s, &lv_font_montserrat_14, COL_DIM, LV_ALIGN_TOP_MID, 0, 168, "Humidity");
    s_s_status = make_label(s, &lv_font_montserrat_14, COL_DIM, LV_ALIGN_BOTTOM_MID, 0, -8, "No sensor data");

    lv_screen_load(s_scr[UI_SCREEN_FACE]);
    return ESP_OK;
}

lv_obj_t *ui_face_parent(void) { return s_scr[UI_SCREEN_FACE]; }
ui_screen_t ui_current(void) { return s_cur; }

void ui_show(ui_screen_t screen)
{
    if (screen == s_cur) return;
#if CONFIG_ROBOT_TRANSITION_SLIDE
    lv_screen_load_anim_t a = screen == UI_SCREEN_FACE ? LV_SCREEN_LOAD_ANIM_MOVE_RIGHT : LV_SCREEN_LOAD_ANIM_MOVE_LEFT;
#else
    lv_screen_load_anim_t a = LV_SCREEN_LOAD_ANIM_FADE_IN;
#endif
    lv_screen_load_anim(s_scr[screen], a, CONFIG_ROBOT_TRANSITION_MS, 0, false);
    s_cur = screen;
}

static void fmt_age(char *buf, size_t n, int64_t ts_ms)
{
    int64_t s = (esp_timer_get_time() / 1000 - ts_ms) / 1000;
    if (s < 60) snprintf(buf, n, "%llds ago", (long long)s);
    else if (s < 3600) snprintf(buf, n, "%lldm ago", (long long)(s / 60));
    else snprintf(buf, n, "%lldh ago", (long long)(s / 3600));
}

void ui_update_weather(const weather_data_t *w, wifi_status_t wifi)
{
    char buf[64], age[16];
    snprintf(buf, sizeof(buf), LV_SYMBOL_WIFI " %s", connectivity_status_str(wifi));
    set_text(s_w_wifi, buf);
    set_color(s_w_wifi, wifi == WIFI_STATUS_CONNECTED ? 0x5BD75B : COL_WARN);

    if (!w->valid) {
        set_text(s_w_cond, wifi == WIFI_STATUS_DISABLED ? "Wi-Fi not configured" : "Waiting for weather");
        return;
    }
    const weather_current_t *c = &w->cur;
    icon_set(weather_code_category(c->weather_code), c->is_day);
    snprintf(buf, sizeof(buf), "%.1f\xC2\xB0" "C", c->temperature);
    set_text(s_w_temp, buf);
    set_text(s_w_cond, weather_code_text(c->weather_code));
    snprintf(buf, sizeof(buf), "Humidity %.0f%%   Wind %.0f km/h", c->humidity, c->wind_speed);
    set_text(s_w_detail, buf);
    // API returns local time "YYYY-MM-DDTHH:MM" -> show HH:MM
    fmt_age(age, sizeof(age), w->updated_ms);
    snprintf(buf, sizeof(buf), "Updated %s (%s)%s", strlen(c->time) >= 16 ? c->time + 11 : "--:--",
             age, w->stale ? " - stale" : "");
    set_text(s_w_updated, buf);
    set_color(s_w_updated, w->stale ? COL_WARN : COL_DIM);
}

void ui_update_sensor(const sensor_data_t *s)
{
    char buf[48], age[16];
#if !CONFIG_ROBOT_ENABLE_SENSOR
    set_text(s_s_status, "Sensor disabled");
    return;
#endif
    if (s->timestamp_ms == 0) {
        if (s->fail_count) snprintf(buf, sizeof(buf), "Sensor error (x%lu)", (unsigned long)s->fail_count);
        else snprintf(buf, sizeof(buf), "Reading sensor...");
        set_text(s_s_status, buf);
        set_color(s_s_status, s->fail_count ? COL_WARN : COL_DIM);
        return;
    }
    snprintf(buf, sizeof(buf), "%.1f\xC2\xB0" "C", s->temperature);
    set_text(s_s_temp, buf);
    snprintf(buf, sizeof(buf), "%.0f%%", s->humidity);
    set_text(s_s_hum, buf);
    fmt_age(age, sizeof(age), s->timestamp_ms);
    snprintf(buf, sizeof(buf), "Sensor %s - %s", s->valid ? "OK" : "STALE", age);
    set_text(s_s_status, buf);
    set_color(s_s_status, s->valid ? COL_DIM : COL_WARN);
}
