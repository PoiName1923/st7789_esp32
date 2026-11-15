#pragma once
#include <stdbool.h>
#include <stdint.h>
#include "robot_settings.h"

typedef enum { ROBOT_COMMAND_SET_SCREEN, ROBOT_COMMAND_PLAY_ANIMATION, ROBOT_COMMAND_APPLY_SETTINGS,
               ROBOT_COMMAND_RELOAD_ANIMATION } robot_command_type_t;
typedef struct {
    robot_command_type_t type;
    union {
        robot_screen_mode_t screen;
        uint8_t animation_id;
        robot_settings_patch_t patch;
    };
} robot_command_t;

bool robot_command_validate(const robot_command_t *command);
