/*
 * Host tests for the hardware-independent logic. No ESP-IDF needed:
 *   sh tests/host/run.sh        (gcc/clang/zig cc)
 */
#include <stdio.h>
#include <string.h>
#include <math.h>
#include "display_orient.h"
#include "board_pins.h"
#include "dht11_proto.h"
#include "weather_parse.h"
#include "backoff.h"
#include "robot_fsm.h"
#include "gif_assets.h"
#include "animation_types.h"
#include "robot_settings.h"
#include "connectivity_state.h"
#include "robot_command.h"
#include "web_auth.h"

static int g_fail, g_pass;
#define CHECK(c) do { if (c) g_pass++; else { g_fail++; printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #c); } } while (0)
#define NEAR(a, b) (fabs((double)(a) - (double)(b)) < 1e-3)

static void test_display_orient(void)
{
    // 240x240 panel on a 240x320 ST7789: the 80 unused rows matter when rows are mirrored.
    display_orient_t o = display_orient_get(0, 240, 240, 240, 320, 0, 0);
    CHECK(o.valid && !o.swap_xy && !o.mirror_x && !o.mirror_y && o.gap_x == 0 && o.gap_y == 0);
    o = display_orient_get(90, 240, 240, 240, 320, 0, 0);
    CHECK(o.valid && o.swap_xy && o.mirror_x && !o.mirror_y && o.gap_x == 0 && o.gap_y == 0);
    o = display_orient_get(180, 240, 240, 240, 320, 0, 0);
    CHECK(o.valid && !o.swap_xy && o.mirror_x && o.mirror_y && o.gap_x == 0 && o.gap_y == 80);
    o = display_orient_get(270, 240, 240, 240, 320, 0, 0);
    CHECK(o.valid && o.swap_xy && !o.mirror_x && o.mirror_y && o.gap_x == 80 && o.gap_y == 0);
    // Full-size 240x320 panel: no extra gap at any rotation.
    o = display_orient_get(180, 240, 320, 240, 320, 0, 0);
    CHECK(o.gap_x == 0 && o.gap_y == 0);
    o = display_orient_get(270, 320, 240, 240, 320, 0, 0);
    CHECK(o.gap_x == 0 && o.gap_y == 0);
    // User calibration offsets are added.
    o = display_orient_get(180, 240, 240, 240, 320, 2, 1);
    CHECK(o.gap_x == 2 && o.gap_y == 81);
    // Invalid rotation.
    CHECK(!display_orient_get(45, 240, 240, 240, 320, 0, 0).valid);
}

static void test_pins(void)
{
    int defaults[] = { 11, 12, 10, 14, 13, -1, 4 };   // MOSI SCLK CS DC RST BL DHT
    for (unsigned i = 0; i < sizeof(defaults) / sizeof(defaults[0]); i++)
        CHECK(board_pin_classify(defaults[i]) == PIN_OK || defaults[i] == -1);
    CHECK(board_pins_find_duplicate(defaults, 7) == -1);
    int dup[] = { 11, 12, -1, -1, 12 };
    CHECK(board_pins_find_duplicate(dup, 5) == 12);       // -1 repeated is fine, 12 is not
    CHECK(board_pin_classify(-1) == PIN_UNUSED);
    CHECK(board_pin_classify(22) == PIN_ERR_INVALID);
    CHECK(board_pin_classify(49) == PIN_ERR_INVALID);
    for (int g = 26; g <= 37; g++) CHECK(board_pin_classify(g) == PIN_ERR_FLASH_PSRAM);
    CHECK(board_pin_classify(0) == PIN_WARN_STRAPPING);
    CHECK(board_pin_classify(46) == PIN_WARN_STRAPPING);
    CHECK(board_pin_classify(19) == PIN_WARN_USB);
    CHECK(board_pin_classify(43) == PIN_WARN_UART0);
    CHECK(board_pin_classify(38) == PIN_OK);
}

static void test_dht11(void)
{
    float t = -99, h = -99;
    uint8_t ok[5] = { 55, 0, 24, 5, 84 };              // 55.0 %RH, 24.5 C
    CHECK(dht11_decode(ok, &t, &h) == DHT11_OK && NEAR(t, 24.5) && NEAR(h, 55.0));

    uint8_t bad_sum[5] = { 55, 0, 24, 5, 85 };
    t = h = -99;
    CHECK(dht11_decode(bad_sum, &t, &h) == DHT11_ERR_CHECKSUM && t == -99 && h == -99);  // untouched

    uint8_t zero[5] = { 0 };
    CHECK(dht11_decode(zero, &t, &h) == DHT11_ERR_EMPTY);

    uint8_t hot[5] = { 50, 0, 70, 0, 120 };            // 70 C: out of range
    CHECK(dht11_decode(hot, &t, &h) == DHT11_ERR_RANGE);

    uint8_t neg[5] = { 40, 0, 2, 0x83, (uint8_t)(40 + 2 + 0x83) };   // -2.3 C
    CHECK(dht11_decode(neg, &t, &h) == DHT11_OK && NEAR(t, -2.3));

    uint8_t wrap[5] = { 90, 9, 50, 9, (uint8_t)(90 + 9 + 50 + 9) };  // checksum wraps past 255
    CHECK(dht11_decode(wrap, &t, &h) == DHT11_OK && NEAR(h, 90.9));

    // Pulse widths -> bytes: 0x37 (55) = 00110111
    uint16_t pulses[DHT11_BITS];
    uint8_t out[5];
    for (int i = 0; i < DHT11_BITS; i++) pulses[i] = 26;
    const uint8_t want0 = 0x37;
    for (int b = 0; b < 8; b++) if (want0 & (0x80 >> b)) pulses[b] = 70;
    pulses[39] = 70;                                   // last bit of checksum
    dht11_bits_to_bytes(pulses, out);
    CHECK(out[0] == 0x37 && out[1] == 0 && out[4] == 0x01);
}

static const char *OPEN_METEO =
    "{\"latitude\":10.875,\"longitude\":106.625,\"generationtime_ms\":0.05,\"utc_offset_seconds\":25200,"
    "\"timezone\":\"Asia/Bangkok\",\"timezone_abbreviation\":\"GMT+7\",\"elevation\":9.0,"
    "\"current_units\":{\"time\":\"iso8601\",\"interval\":\"seconds\",\"temperature_2m\":\"\xC2\xB0" "C\","
    "\"relative_humidity_2m\":\"%\",\"weather_code\":\"wmo code\",\"is_day\":\"\",\"wind_speed_10m\":\"km/h\"},"
    "\"current\":{\"time\":\"2026-11-12T14:30\",\"interval\":900,\"temperature_2m\":31.4,"
    "\"relative_humidity_2m\":66,\"weather_code\":80,\"is_day\":1,\"wind_speed_10m\":9.7}}";

static void test_weather(void)
{
    weather_current_t w;
    memset(&w, 0, sizeof(w));
    CHECK(weather_parse_open_meteo(OPEN_METEO, &w) == 0);
    CHECK(NEAR(w.temperature, 31.4) && NEAR(w.humidity, 66) && w.weather_code == 80 && w.is_day);
    CHECK(NEAR(w.wind_speed, 9.7) && strcmp(w.time, "2026-11-12T14:30") == 0);

    // Whitespace + negative values + night.
    const char *spaced = "{ \"current\" : { \"time\" : \"2026-01-01T03:00\", \"temperature_2m\" : -4.5 ,"
                         " \"relative_humidity_2m\": 91, \"weather_code\": 73, \"is_day\": 0, \"wind_speed_10m\": 0 } }";
    CHECK(weather_parse_open_meteo(spaced, &w) == 0 && NEAR(w.temperature, -4.5) && !w.is_day && w.weather_code == 73);

    weather_current_t keep = w;
    CHECK(weather_parse_open_meteo("{\"current\":{\"temperature_2m\":20}}", &w) == -1);           // missing fields
    CHECK(weather_parse_open_meteo("{\"error\":true,\"reason\":\"bad lat\"}", &w) == -1);          // API error
    CHECK(weather_parse_open_meteo("{\"current_units\":{\"temperature_2m\":\"C\"}}", &w) == -1);   // units only
    CHECK(weather_parse_open_meteo("{\"current\":{\"temperature_2m\":null,\"relative_humidity_2m\":1,"
                                   "\"weather_code\":0,\"is_day\":1,\"wind_speed_10m\":1}}", &w) == -1);
    CHECK(weather_parse_open_meteo("{\"current\":{\"temperature_2m\":1", &w) == -1);              // truncated
    CHECK(weather_parse_open_meteo("", &w) == -1 && weather_parse_open_meteo(NULL, &w) == -1);
    CHECK(memcmp(&keep, &w, sizeof(w)) == 0);                                                    // untouched on error

    // WMO code mapping.
    CHECK(weather_code_category(0) == WX_CLEAR && weather_code_category(1) == WX_CLEAR);
    CHECK(weather_code_category(2) == WX_PARTLY_CLOUDY && weather_code_category(3) == WX_CLOUDY);
    CHECK(weather_code_category(45) == WX_FOG && weather_code_category(48) == WX_FOG);
    CHECK(weather_code_category(53) == WX_DRIZZLE && weather_code_category(57) == WX_DRIZZLE);
    CHECK(weather_code_category(63) == WX_RAIN && weather_code_category(82) == WX_RAIN);
    CHECK(weather_code_category(75) == WX_SNOW && weather_code_category(86) == WX_SNOW);
    CHECK(weather_code_category(95) == WX_THUNDER && weather_code_category(99) == WX_THUNDER);
    CHECK(weather_code_category(4) == WX_UNKNOWN && weather_code_category(-1) == WX_UNKNOWN);
    CHECK(strcmp(weather_code_text(80), "Rain showers") == 0 && strcmp(weather_code_text(1234), "Unknown") == 0);
    for (int c = 0; c < 100; c++) CHECK(weather_code_text(c) != NULL);
}

static void test_backoff(void)
{
    CHECK(backoff_delay_ms(0, 1000, 300000) == 1000);
    CHECK(backoff_delay_ms(1, 1000, 300000) == 2000);
    CHECK(backoff_delay_ms(5, 1000, 300000) == 32000);
    CHECK(backoff_delay_ms(9, 1000, 300000) == 300000);   // 512000 capped
    CHECK(backoff_delay_ms(31, 1000, 300000) == 300000);
    CHECK(backoff_delay_ms(1000, 1000, 300000) == 300000);
    uint32_t prev = 0;
    for (uint32_t a = 0; a < 64; a++) { uint32_t d = backoff_delay_ms(a, 1000, 300000); CHECK(d >= prev && d <= 300000); prev = d; }
}

static const robot_fsm_cfg_t CFG = {
    .idle_ms = 8000, .expression_ms = 2500, .blink_ms = 250, .blink_min_ms = 2000, .blink_max_ms = 6000,
    .info_interval_ms = 30000, .info_ms = 6000, .weather_enabled = true, .sensor_enabled = true,
};

typedef struct { int count[RS_COUNT]; int bad_transitions, bad_duration, bad_blink_gap, bad_alternation; } fsm_stats_t;

static fsm_stats_t run_fsm(const robot_fsm_cfg_t *cfg, uint32_t start, uint32_t duration_ms, uint32_t seed)
{
    const uint32_t step = 50;
    fsm_stats_t st;
    memset(&st, 0, sizeof(st));
    robot_fsm_t f;
    robot_fsm_init(&f, cfg, start, seed);
    uint32_t entered = start, idle_since = start;
    robot_state_t last_info = RS_COUNT;
    for (uint32_t t = start + step; t - start < duration_ms; t += step) {
        robot_state_t prev = f.state;
        if (!robot_fsm_tick(&f, t)) continue;
        robot_state_t s = f.state;
        st.count[s]++;
        if (prev != RS_IDLE && s != RS_IDLE) st.bad_transitions++;   // always via IDLE
        if (prev == RS_IDLE && s == RS_IDLE) st.bad_transitions++;   // change must be a change
        if (prev != RS_IDLE) {                                        // left a timed state
            uint32_t want = prev == RS_BLINKING ? cfg->blink_ms
                          : (prev == RS_WEATHER || prev == RS_SENSOR) ? cfg->info_ms : cfg->expression_ms;
            if (t - entered < want || t - entered > want + step) st.bad_duration++;
            idle_since = t;
        }
        if (s == RS_BLINKING && (t - idle_since < cfg->blink_min_ms || t - idle_since > cfg->blink_max_ms + step))
            st.bad_blink_gap++;
        if (s == RS_WEATHER || s == RS_SENSOR) {
            if (cfg->weather_enabled && cfg->sensor_enabled && s == last_info) st.bad_alternation++;
            last_info = s;
        }
        entered = t;
    }
    return st;
}

static void test_fsm(void)
{
    fsm_stats_t s = run_fsm(&CFG, 0, 30 * 60 * 1000, 42);
    CHECK(s.bad_transitions == 0);
    CHECK(s.bad_duration == 0);
    CHECK(s.bad_blink_gap == 0);
    CHECK(s.bad_alternation == 0);
    CHECK(s.count[RS_BLINKING] > 100);
    CHECK(s.count[RS_WEATHER] > 15 && s.count[RS_SENSOR] > 15);
    int expr = s.count[RS_HAPPY] + s.count[RS_SLEEPY] + s.count[RS_SURPRISED] + s.count[RS_LOOK_LEFT] + s.count[RS_LOOK_RIGHT];
    CHECK(expr > 20);
    CHECK(s.count[RS_HAPPY] > 0 && s.count[RS_SLEEPY] > 0 && s.count[RS_SURPRISED] > 0 &&
          s.count[RS_LOOK_LEFT] > 0 && s.count[RS_LOOK_RIGHT] > 0);

    robot_fsm_cfg_t c = CFG;
    c.weather_enabled = c.sensor_enabled = false;
    s = run_fsm(&c, 0, 10 * 60 * 1000, 7);
    CHECK(s.count[RS_WEATHER] == 0 && s.count[RS_SENSOR] == 0 && s.count[RS_BLINKING] > 0);

    c = CFG;
    c.sensor_enabled = false;
    s = run_fsm(&c, 0, 10 * 60 * 1000, 7);
    CHECK(s.count[RS_SENSOR] == 0 && s.count[RS_WEATHER] > 5);

    // lv_tick_get() wraps after ~49 days: behaviour must continue across the wrap.
    s = run_fsm(&CFG, 0xFFFFFFFFu - 20000, 120000, 3);
    CHECK(s.bad_transitions == 0 && s.bad_duration == 0 && s.count[RS_BLINKING] > 5 && s.count[RS_WEATHER] > 0);

    // Fresh FSM is idle and silent until the first blink is due.
    robot_fsm_t f;
    robot_fsm_init(&f, &CFG, 1000, 1);
    CHECK(f.state == RS_IDLE && !robot_fsm_tick(&f, 1000 + CFG.blink_min_ms - 1));
    CHECK(strcmp(robot_state_name(RS_WEATHER), "WEATHER") == 0 && strcmp(robot_state_name(RS_COUNT), "?") == 0);
}

static void test_gif_assets(void)
{
    static const uint8_t gif[] = "GIF89a\x10\x00\x10\x00\x80\x00\x00;";
    static const uint8_t junk[] = "PNG\x89 not a gif at all";
    const gif_asset_t table[] = {
        { "robot_idle", gif, gif + sizeof(gif) - 1 },
        { "robot_happy", junk, junk + sizeof(junk) - 1 },
        { 0 },
    };
    CHECK(gif_asset_find(table, 2, "robot_idle") == &table[0]);
    CHECK(gif_asset_find(table, 2, "robot_sleepy") == NULL);          // missing file -> fallback
    CHECK(gif_asset_find(table, 0, "robot_idle") == NULL);            // no assets at all
    CHECK(gif_header_valid(gif, sizeof(gif) - 1));
    CHECK(!gif_header_valid(junk, sizeof(junk) - 1));                 // corrupt -> fallback
    CHECK(!gif_header_valid(gif, 5) && !gif_header_valid(NULL, 100));
}

static void test_animation_names(void)
{
    CHECK(strcmp(animation_name(ROBOT_IDLE), "robot_idle") == 0);
    CHECK(strcmp(animation_name((animation_id_t)9), "uploaded GIF") == 0);
    CHECK(strcmp(animation_name((animation_id_t)40), "uploaded GIF") == 0);
    CHECK(strcmp(animation_name(ANIMATION_COUNT), "?") == 0);
}

static void test_settings(void)
{
    robot_settings_t s;
    settings_defaults(&s);
    CHECK(settings_validate(&s) == SETTINGS_OK);
    CHECK(s.screen_mode == ROBOT_SCREEN_FACE);
    CHECK(s.animation_fps == 25 && s.screen_switch_s == 30 && s.info_screen_s == 6);
    CHECK(s.display_rotation == 0 && !s.display_bgr && s.display_invert);
    CHECK(strcmp(s.weather_timezone, "auto") == 0 && s.weather_interval_min == 15);

    strcpy(s.ap_password, "short");
    CHECK(settings_validate(&s) == SETTINGS_ERR_AP_PASSWORD);
    strcpy(s.ap_password, "12345678");
    CHECK(settings_validate(&s) == SETTINGS_OK);
    memset(s.ap_password, 'a', 63); s.ap_password[63] = '\0';
    CHECK(settings_validate(&s) == SETTINGS_OK);
    s.ap_password[0] = (char)0xC3;
    CHECK(settings_validate(&s) == SETTINGS_ERR_AP_PASSWORD);

    settings_defaults(&s);
    strcpy(s.wifi_ssid, "open-network");
    s.wifi_password[0] = '\0';
    CHECK(settings_validate(&s) == SETTINGS_OK);
    s.display_rotation = 45;
    CHECK(settings_validate(&s) == SETTINGS_ERR_DISPLAY);
    s.display_rotation = 270;
    s.animation_fps = 4;
    CHECK(settings_validate(&s) == SETTINGS_ERR_ANIMATION);
    s.animation_fps = 50;
    s.weather_interval_min = 4;
    CHECK(settings_validate(&s) == SETTINGS_ERR_WEATHER);
    settings_defaults(&s);
    strcpy(s.wifi_password, "1234567");
    CHECK(settings_validate(&s) == SETTINGS_ERR_WIFI);
    settings_defaults(&s);
    memset(s.weather_timezone, 'x', sizeof(s.weather_timezone));
    CHECK(settings_validate(&s) == SETTINGS_ERR_WEATHER);
    settings_defaults(&s);
    strcpy(s.weather_latitude, "91");
    CHECK(settings_validate(&s) == SETTINGS_ERR_WEATHER);

    settings_defaults(&s);
    strcpy(s.wifi_password, "keep-me");
    robot_settings_patch_t p = { .fields = SETTINGS_FIELD_ANIMATION_FPS, .animation_fps = 30 };
    settings_change_t change = {0};
    CHECK(settings_apply_patch(&s, &p, &change) == SETTINGS_OK);
    CHECK(s.animation_fps == 30 && strcmp(s.wifi_password, "keep-me") == 0);
    CHECK(!change.restart_required && !change.network_changed);

    p = (robot_settings_patch_t){ .fields = SETTINGS_FIELD_DISPLAY_ROTATION, .display_rotation = 180 };
    CHECK(settings_apply_patch(&s, &p, &change) == SETTINGS_OK);
    CHECK(change.restart_required && s.display_rotation == 180);
}

static void test_connectivity_state(void)
{
    connectivity_state_t s;
    connectivity_state_init(&s, false);
    CHECK(!s.ap_ready && s.sta == WIFI_STATUS_DISABLED);
    connectivity_state_init(&s, true);
    CHECK(!s.ap_ready && s.sta == WIFI_STATUS_CONNECTING);
    connectivity_state_event(&s, CONNECTIVITY_EVENT_STA_GOT_IP, 0);
    CHECK(!s.ap_ready && s.sta == WIFI_STATUS_CONNECTED && s.last_error == 0);
    connectivity_state_event(&s, CONNECTIVITY_EVENT_STA_DISCONNECTED, 201);
    CHECK(!s.ap_ready && s.sta == WIFI_STATUS_CONNECTING && s.last_error == 201);
    connectivity_state_event(&s, CONNECTIVITY_EVENT_CREDENTIALS_CHANGED, 0);
    CHECK(!s.ap_ready && s.sta == WIFI_STATUS_CONNECTING);
}

static void test_robot_command(void)
{
    robot_command_t c = { .type = ROBOT_COMMAND_SET_SCREEN, .screen = ROBOT_SCREEN_FACE };
    CHECK(robot_command_validate(&c));
    c.screen = (robot_screen_mode_t)99;
    CHECK(!robot_command_validate(&c));
    c = (robot_command_t){ .type = ROBOT_COMMAND_PLAY_ANIMATION, .animation_id = 0 };
    CHECK(robot_command_validate(&c));
    c.animation_id = 255;
    CHECK(!robot_command_validate(&c));
    c = (robot_command_t){ .type = ROBOT_COMMAND_RELOAD_ANIMATION, .animation_id = 8 };
    CHECK(robot_command_validate(&c));
    c.animation_id = 41;
    CHECK(!robot_command_validate(&c));
    c = (robot_command_t){ .type = ROBOT_COMMAND_APPLY_SETTINGS,
                           .patch = { .fields = SETTINGS_FIELD_DISPLAY_ROTATION, .display_rotation = 180 } };
    CHECK(robot_command_validate(&c));
}

static void test_web_auth(void)
{
    web_auth_t auth;
    web_auth_init(&auth);
    web_session_t *session = web_auth_create(&auth, 1000, 0x12345678);
    CHECK(session && web_auth_find(&auth, session->token, 1001) == session);
    CHECK(web_auth_authorize(&auth, session->token, session->csrf, 1002));
    CHECK(!web_auth_authorize(&auth, session->token, "wrong", 1002));
    CHECK(web_auth_find(&auth, "short", 1002) == NULL);
    CHECK(web_auth_find(&auth, session->token, 1000 + WEB_SESSION_TTL_MS + 1) == NULL);
    session = web_auth_create(&auth, 2000, 7);
    web_auth_invalidate_all(&auth);
    CHECK(web_auth_find(&auth, session->token, 2001) == NULL);
    CHECK(web_auth_login_allowed(&auth, 0));
    web_auth_login_failed(&auth, 0);
    web_auth_login_failed(&auth, 1);
    web_auth_login_failed(&auth, 2);
    CHECK(!web_auth_login_allowed(&auth, 3));
    CHECK(web_auth_login_allowed(&auth, WEB_LOGIN_BLOCK_MS + 3));
}

int main(void)
{
    test_display_orient();
    test_pins();
    test_dht11();
    test_weather();
    test_backoff();
    test_fsm();
    test_gif_assets();
    test_animation_names();
    test_settings();
    test_connectivity_state();
    test_robot_command();
    test_web_auth();
    printf("%d passed, %d failed\n", g_pass, g_fail);
    return g_fail != 0;
}
