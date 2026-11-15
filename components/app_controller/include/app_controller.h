#pragma once
#include "esp_err.h"
#include "lvgl.h"
#include "robot_command.h"

/**
 * Start the behaviour loop (an LVGL timer, so all UI work happens in LVGL context),
 * the heap/FPS monitor and, if enabled, the on-target self-test.
 * Call after ui_init() + animation_init(). Takes the LVGL lock itself.
 */
esp_err_t app_controller_start(lv_display_t *disp);
bool app_controller_send(const robot_command_t *command, uint32_t timeout_ms);
