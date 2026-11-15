#pragma once
// Pure logic (no ESP-IDF deps) -> host-testable.
#include <stdbool.h>

typedef struct {
    bool swap_xy, mirror_x, mirror_y;
    int gap_x, gap_y;
    bool valid;
} display_orient_t;

/**
 * Map a rotation (0/90/180/270) to ST7789 MADCTL flags + RAM gap.
 * ram_w/ram_h: controller RAM size (ST7789: 240x320). A 240x240 panel only uses
 * the first 240 rows, so when the row axis is mirrored the window shifts by 80.
 * off_x/off_y: user calibration added on top.
 */
display_orient_t display_orient_get(int rotation_deg, int w, int h,
                                    int ram_w, int ram_h, int off_x, int off_y);
