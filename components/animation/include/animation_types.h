#pragma once

typedef enum {
    ROBOT_IDLE,
    ROBOT_BLINK,
    ROBOT_HAPPY,
    ROBOT_SLEEPY,
    ROBOT_SURPRISED,
    ROBOT_LOOK_LEFT,
    ROBOT_LOOK_RIGHT,
    ROBOT_WEATHER_SUNNY,
    ROBOT_WEATHER_RAIN,
    ANIMATION_COUNT = 41  // 9 standard expressions + 32 imported GIF slots
} animation_id_t;

const char *animation_name(animation_id_t id);
