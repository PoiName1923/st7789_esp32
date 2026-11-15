#pragma once
#include "esp_err.h"
#include "lvgl.h"

/**
 * Validate pins, bring up SPI + ST7789, show the colour-bar test pattern,
 * then start esp_lvgl_port (its task owns all LVGL rendering).
 * After this, touch LVGL only inside lvgl_port_lock()/unlock() or from LVGL timers.
 */
esp_err_t display_init(lv_display_t **out_disp);

/** Validate all configured GPIOs (display + DHT11). Logs warnings, fails on hard errors. */
esp_err_t display_check_pins(void);
