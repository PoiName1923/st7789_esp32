#pragma once
// Pure behaviour state machine (no ESP-IDF/LVGL deps) -> host-testable.
// Driven by robot_fsm_tick(now_ms); never blocks, never renders.
#include <stdbool.h>
#include <stdint.h>

typedef enum {
    RS_IDLE, RS_BLINKING, RS_HAPPY, RS_SLEEPY, RS_SURPRISED,
    RS_LOOK_LEFT, RS_LOOK_RIGHT, RS_WEATHER, RS_SENSOR, RS_COUNT
} robot_state_t;

typedef struct {
    uint32_t idle_ms;           // idle time before a random alternate expression
    uint32_t expression_ms;     // how long an expression lasts
    uint32_t blink_ms;          // blink duration
    uint32_t blink_min_ms, blink_max_ms;
    uint32_t info_interval_ms;  // face time between info screens
    uint32_t info_ms;           // how long an info screen stays
    bool weather_enabled, sensor_enabled;
} robot_fsm_cfg_t;

typedef struct {
    robot_fsm_cfg_t cfg;
    robot_state_t state;
    uint32_t state_until, next_blink, next_expression, next_info;
    bool last_info_weather;
    uint32_t rng;
} robot_fsm_t;

void robot_fsm_init(robot_fsm_t *f, const robot_fsm_cfg_t *cfg, uint32_t now_ms, uint32_t seed);
/** Advance time. Returns true if the state changed (read f->state). Wrap-safe. */
bool robot_fsm_tick(robot_fsm_t *f, uint32_t now_ms);
const char *robot_state_name(robot_state_t s);
