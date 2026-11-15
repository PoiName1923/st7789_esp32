#include "robot_fsm.h"

static bool due(uint32_t now, uint32_t t) { return (int32_t)(now - t) >= 0; }

static uint32_t rnd(robot_fsm_t *f)   // xorshift32
{
    uint32_t x = f->rng;
    x ^= x << 13; x ^= x >> 17; x ^= x << 5;
    return f->rng = x;
}

static uint32_t rnd_between(robot_fsm_t *f, uint32_t lo, uint32_t hi)
{
    return hi <= lo ? lo : lo + rnd(f) % (hi - lo + 1);
}

static void enter(robot_fsm_t *f, robot_state_t s, uint32_t now, uint32_t dur)
{
    f->state = s;
    f->state_until = now + dur;
}

void robot_fsm_init(robot_fsm_t *f, const robot_fsm_cfg_t *cfg, uint32_t now, uint32_t seed)
{
    *f = (robot_fsm_t){ .cfg = *cfg, .state = RS_IDLE, .rng = seed ? seed : 0x12345678u };
    f->next_blink = now + rnd_between(f, cfg->blink_min_ms, cfg->blink_max_ms);
    f->next_expression = now + cfg->idle_ms;
    f->next_info = now + cfg->info_interval_ms;
}

bool robot_fsm_tick(robot_fsm_t *f, uint32_t now)
{
    const robot_fsm_cfg_t *c = &f->cfg;

    if (f->state == RS_IDLE) {
        if ((c->weather_enabled || c->sensor_enabled) && due(now, f->next_info)) {
            bool weather = c->weather_enabled && (!c->sensor_enabled || !f->last_info_weather);
            f->last_info_weather = weather;
            enter(f, weather ? RS_WEATHER : RS_SENSOR, now, c->info_ms);
            return true;
        }
        if (due(now, f->next_expression)) {
            static const robot_state_t pool[] = { RS_HAPPY, RS_SLEEPY, RS_SURPRISED, RS_LOOK_LEFT, RS_LOOK_RIGHT };
            enter(f, pool[rnd(f) % (sizeof(pool) / sizeof(pool[0]))], now, c->expression_ms);
            return true;
        }
        if (due(now, f->next_blink)) {
            enter(f, RS_BLINKING, now, c->blink_ms);
            return true;
        }
        return false;
    }

    if (!due(now, f->state_until)) return false;

    // Back to idle; reschedule whatever just ran.
    if (f->state == RS_WEATHER || f->state == RS_SENSOR) f->next_info = now + c->info_interval_ms;
    if (f->state != RS_BLINKING) f->next_expression = now + c->idle_ms;   // blinks don't reset the idle clock
    f->next_blink = now + rnd_between(f, c->blink_min_ms, c->blink_max_ms);
    f->state = RS_IDLE;
    return true;
}

const char *robot_state_name(robot_state_t s)
{
    static const char *const n[RS_COUNT] = {
        "IDLE", "BLINKING", "HAPPY", "SLEEPY", "SURPRISED", "LOOK_LEFT", "LOOK_RIGHT", "WEATHER", "SENSOR",
    };
    return (unsigned)s < RS_COUNT ? n[s] : "?";
}
