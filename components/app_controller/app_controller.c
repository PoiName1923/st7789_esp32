#include "app_controller.h"
#include "animation.h"
#include "ui.h"
#include "sensor.h"
#include "weather.h"
#include "connectivity.h"
#include "esp_lvgl_port.h"
#include "esp_heap_caps.h"
#include "esp_timer.h"
#include "esp_wifi.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "robot_settings.h"
#include "robot_fsm.h"
#include "esp_random.h"

#define TICK_MS          100

static const char *TAG = "app";

static lv_timer_t *s_tick_timer;
static volatile uint32_t s_frames;
static int64_t s_render_us, s_render_t0;
static QueueHandle_t s_commands;
static void update_info(ui_screen_t scr);
static lv_display_t *s_display;
static robot_screen_mode_t s_mode;
static robot_fsm_t s_face_fsm;
static bool s_face_batch;

static animation_id_t face_state_animation(robot_state_t state)
{
    static const animation_id_t map[RS_COUNT] = {
        [RS_IDLE] = ROBOT_IDLE, [RS_BLINKING] = ROBOT_BLINK,
        [RS_HAPPY] = ROBOT_HAPPY, [RS_SLEEPY] = ROBOT_SLEEPY,
        [RS_SURPRISED] = ROBOT_SURPRISED, [RS_LOOK_LEFT] = ROBOT_LOOK_LEFT,
        [RS_LOOK_RIGHT] = ROBOT_LOOK_RIGHT,
    };
    return state < RS_COUNT ? map[state] : ROBOT_IDLE;
}

static void face_batch_start(animation_id_t first)
{
    const robot_fsm_cfg_t cfg = {
        .idle_ms = 8000, .expression_ms = 2500, .blink_ms = 250,
        .blink_min_ms = 2000, .blink_max_ms = 6000,
        .weather_enabled = false, .sensor_enabled = false,
    };
    robot_fsm_init(&s_face_fsm, &cfg, lv_tick_get(), esp_random());
    s_face_batch = true;
    ui_show(UI_SCREEN_FACE);
    animation_set_loop(true);
    animation_play(first);
}

static void apply_command(const robot_command_t *c)
{
    if (c->type == ROBOT_COMMAND_SET_SCREEN) {
        robot_settings_patch_t patch = { .fields = SETTINGS_FIELD_SCREEN_MODE, .screen_mode = c->screen };
        settings_change_t change;
        if (settings_update(&patch, &change) != ESP_OK) return;
        s_mode = c->screen;
        if (c->screen == ROBOT_SCREEN_AUTO || c->screen == ROBOT_SCREEN_FACE) {
            face_batch_start(ROBOT_IDLE);
        } else if (c->screen == ROBOT_SCREEN_WEATHER) {
            s_face_batch = false; update_info(UI_SCREEN_WEATHER); animation_stop(); ui_show(UI_SCREEN_WEATHER);
        } else if (c->screen == ROBOT_SCREEN_SENSOR) {
            s_face_batch = false; update_info(UI_SCREEN_SENSOR); animation_stop(); ui_show(UI_SCREEN_SENSOR);
        }
    } else if (c->type == ROBOT_COMMAND_PLAY_ANIMATION) {
        robot_settings_patch_t patch = { .fields = SETTINGS_FIELD_ANIMATION_ID | SETTINGS_FIELD_SCREEN_MODE,
                                         .animation_id = c->animation_id, .screen_mode = ROBOT_SCREEN_FACE };
        settings_change_t change;
        if (settings_update(&patch, &change) != ESP_OK) return;
        s_mode = ROBOT_SCREEN_FACE;
        if (c->animation_id < 9) face_batch_start((animation_id_t)c->animation_id);
        else { s_face_batch = false; ui_show(UI_SCREEN_FACE); animation_set_loop(true); animation_play((animation_id_t)c->animation_id); }
    } else if (c->type == ROBOT_COMMAND_RELOAD_ANIMATION) {
        animation_reload((animation_id_t)c->animation_id);
    } else if (c->type == ROBOT_COMMAND_APPLY_SETTINGS) {
        robot_settings_t settings;
        settings_get(&settings);
        if (c->patch.fields & SETTINGS_FIELD_ANIMATION_FPS)
            lv_timer_set_period(lv_display_get_refr_timer(s_display), 1000 / settings.animation_fps);
        if (c->patch.fields & SETTINGS_FIELD_FACE_COLOR) animation_set_face_color(settings.face_color);
    }
}

static void update_info(ui_screen_t scr)
{
    if (scr == UI_SCREEN_WEATHER) {
        weather_data_t w;
        weather_get(&w);
        ui_update_weather(&w, connectivity_status());
    } else if (scr == UI_SCREEN_SENSOR) {
        sensor_data_t s;
        sensor_get(&s);
        ui_update_sensor(&s);
    }
}

static void tick_cb(lv_timer_t *t)
{
    robot_command_t command;
    while (s_commands && xQueueReceive(s_commands, &command, 0) == pdTRUE) apply_command(&command);
    static uint32_t n;
    if (s_face_batch && robot_fsm_tick(&s_face_fsm, lv_tick_get())) {
        animation_id_t id = face_state_animation(s_face_fsm.state);
        animation_set_loop(id != ROBOT_BLINK);
        animation_play(id);
    }
    if (++n % (1000 / TICK_MS) == 0) update_info(ui_current());   // live values, 1 Hz
}

static void render_evt(lv_event_t *e)
{
    if (lv_event_get_code(e) == LV_EVENT_RENDER_START) {
        s_render_t0 = esp_timer_get_time();
    } else {
        s_frames++;
        s_render_us += esp_timer_get_time() - s_render_t0;
    }
}

static void monitor_cb(lv_timer_t *t)
{
    static uint32_t n;
    uint32_t frames = s_frames;
    ESP_LOGI(TAG, "%.1f fps (avg render %lld us) | int %u KB (min %u, blk %u) | psram %u KB | screen %u / %s [%s]",
             frames / (float)CONFIG_ROBOT_MONITOR_INTERVAL_S,
             frames ? (long long)(s_render_us / frames) : 0LL,
             (unsigned)(heap_caps_get_free_size(MALLOC_CAP_INTERNAL) / 1024),
             (unsigned)(heap_caps_get_minimum_free_size(MALLOC_CAP_INTERNAL) / 1024),
             (unsigned)(heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL) / 1024),
             (unsigned)(heap_caps_get_free_size(MALLOC_CAP_SPIRAM) / 1024),
             (unsigned)s_mode, animation_name(animation_current()),
             animation_source(animation_current()) == ANIM_SRC_GIF ? "gif" : "drawn");
    s_frames = 0;
    s_render_us = 0;
    if (++n % 6 == 0) {   // task list (stack high-water marks) every 6th report
        static char buf[1024];
        vTaskList(buf);
        ESP_LOGI(TAG, "tasks (name state prio stack_free_words num):\n%s", buf);
    }
}

#if CONFIG_ROBOT_SELFTEST
static void selftest_task(void *arg)
{
    vTaskDelay(pdMS_TO_TICKS(3000));
    lvgl_port_lock(0);
    lv_timer_pause(s_tick_timer);
    lvgl_port_unlock();

    // 1. Repeated animation switching must not grow the heap.
    size_t base = 0;
    const int rounds = 21;
    for (int r = 0; r < rounds; r++) {
        for (int id = 0; id < 9; id++) {
            lvgl_port_lock(0);
            animation_set_loop(id != ROBOT_BLINK);
            animation_play((animation_id_t)id);
            lvgl_port_unlock();
            vTaskDelay(pdMS_TO_TICKS(30));
        }
        if (r == 0) {   // first round warms up LVGL caches
            vTaskDelay(pdMS_TO_TICKS(300));
            base = heap_caps_get_free_size(MALLOC_CAP_DEFAULT);
        }
    }
    vTaskDelay(pdMS_TO_TICKS(300));
    long delta = (long)base - (long)heap_caps_get_free_size(MALLOC_CAP_DEFAULT);
    ESP_LOGI(TAG, "SELFTEST switching: %d switches, heap delta %ld B -> %s",
             (rounds - 1) * 9, delta, delta < 4096 && delta > -4096 ? "PASS" : "FAIL");

    lvgl_port_lock(0);
    esp_err_t err = animation_play((animation_id_t)ANIMATION_COUNT);
    lvgl_port_unlock();
    ESP_LOGI(TAG, "SELFTEST invalid id rejected: %s", err == ESP_ERR_INVALID_ARG ? "PASS" : "FAIL");

    // 2. Wi-Fi reconnect: force a disconnect and expect the backoff logic to recover.
    if (connectivity_status() == WIFI_STATUS_CONNECTED) {
        esp_wifi_disconnect();
        for (int i = 0; i < 50 && connectivity_status() == WIFI_STATUS_CONNECTED; i++) vTaskDelay(pdMS_TO_TICKS(100));
        int64_t t0 = esp_timer_get_time();
        bool ok = connectivity_wait(60000);
        ESP_LOGI(TAG, "SELFTEST wifi reconnect: %s in %lld ms", ok ? "PASS" : "FAIL",
                 (long long)((esp_timer_get_time() - t0) / 1000));
    } else {
        ESP_LOGI(TAG, "SELFTEST wifi reconnect: SKIP (not connected)");
    }

    // 3. Sensor sanity (informational).
    sensor_data_t s;
    sensor_get(&s);
    ESP_LOGI(TAG, "SELFTEST sensor: %s (%.1f C, %.0f %%, fails %lu)", s.valid ? "PASS" : "NO DATA",
             s.temperature, s.humidity, (unsigned long)s.fail_count);

    lvgl_port_lock(0);
    animation_set_loop(true);
    animation_play(ROBOT_IDLE);
    lv_timer_resume(s_tick_timer);
    lvgl_port_unlock();
    vTaskDelete(NULL);
}
#endif

esp_err_t app_controller_start(lv_display_t *disp)
{
    s_display = disp;
    robot_settings_t settings;
    settings_get(&settings);
    s_mode = settings.screen_mode == ROBOT_SCREEN_AUTO ? ROBOT_SCREEN_FACE : settings.screen_mode;
    s_commands = xQueueCreate(8, sizeof(robot_command_t));
    if (!s_commands) return ESP_ERR_NO_MEM;
    lvgl_port_lock(0);
    animation_set_face_color(settings.face_color);
    lv_timer_set_period(lv_display_get_refr_timer(disp), 1000 / settings.animation_fps);
    if (s_mode == ROBOT_SCREEN_WEATHER) { update_info(UI_SCREEN_WEATHER); animation_stop(); ui_show(UI_SCREEN_WEATHER); }
    else if (s_mode == ROBOT_SCREEN_SENSOR) { update_info(UI_SCREEN_SENSOR); animation_stop(); ui_show(UI_SCREEN_SENSOR); }
    else if (s_mode == ROBOT_SCREEN_FACE && settings.animation_id >= 9 && settings.animation_id < ANIMATION_COUNT) {
        s_face_batch = false; animation_set_loop(true); animation_play((animation_id_t)settings.animation_id);
    } else face_batch_start(ROBOT_IDLE);
    s_tick_timer = lv_timer_create(tick_cb, TICK_MS, NULL);
    if (CONFIG_ROBOT_MONITOR_INTERVAL_S > 0) {
        lv_display_add_event_cb(disp, render_evt, LV_EVENT_RENDER_START, NULL);
        lv_display_add_event_cb(disp, render_evt, LV_EVENT_RENDER_READY, NULL);
        lv_timer_create(monitor_cb, CONFIG_ROBOT_MONITOR_INTERVAL_S * 1000, NULL);
    }
    lvgl_port_unlock();

#if CONFIG_ROBOT_SELFTEST
    xTaskCreatePinnedToCore(selftest_task, "selftest", 4096, NULL, 2, NULL, 0);
#endif
    return s_tick_timer ? ESP_OK : ESP_ERR_NO_MEM;
}

bool app_controller_send(const robot_command_t *command, uint32_t timeout_ms)
{
    return s_commands && robot_command_validate(command) &&
           xQueueSend(s_commands, command, pdMS_TO_TICKS(timeout_ms)) == pdTRUE;
}
