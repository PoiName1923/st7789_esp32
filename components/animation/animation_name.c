#include "animation_types.h"

const char *animation_name(animation_id_t id)
{
    static const char *const names[] = {
        "robot_idle", "robot_blink", "robot_happy", "robot_sleepy", "robot_surprised",
        "robot_look_left", "robot_look_right", "robot_weather_sunny", "robot_weather_rain",
    };
    return (unsigned)id >= ANIMATION_COUNT ? "?" : id < 9 ? names[id] : "uploaded GIF";
}
