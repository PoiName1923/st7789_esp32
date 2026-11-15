#include "robot_command.h"

bool robot_command_validate(const robot_command_t *c)
{
    if (!c) return false;
    if (c->type == ROBOT_COMMAND_SET_SCREEN) return c->screen >= ROBOT_SCREEN_FACE && c->screen <= ROBOT_SCREEN_SENSOR;
    if (c->type == ROBOT_COMMAND_PLAY_ANIMATION) return c->animation_id < 41;
    if (c->type == ROBOT_COMMAND_RELOAD_ANIMATION) return c->animation_id < 41;
    if (c->type == ROBOT_COMMAND_APPLY_SETTINGS) {
        robot_settings_t s;
        settings_defaults(&s);
        settings_change_t change;
        return settings_apply_patch(&s, &c->patch, &change) == SETTINGS_OK;
    }
    return false;
}
