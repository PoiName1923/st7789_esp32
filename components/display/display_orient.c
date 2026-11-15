#include "display_orient.h"

display_orient_t display_orient_get(int rotation_deg, int w, int h,
                                    int ram_w, int ram_h, int off_x, int off_y)
{
    display_orient_t o = { .gap_x = off_x, .gap_y = off_y, .valid = true };
    switch (rotation_deg) {
    case 0:   break;
    case 90:  o.swap_xy = true;  o.mirror_x = true;  break;
    case 180: o.mirror_x = true; o.mirror_y = true;  break;
    case 270: o.swap_xy = true;  o.mirror_y = true;  break;
    default:  o.valid = false;   return o;
    }
    // Physical panel size (rows/cols as the controller sees them).
    int phys_cols = o.swap_xy ? h : w;
    int phys_rows = o.swap_xy ? w : h;
    int row_extra = o.mirror_y ? ram_h - phys_rows : 0;
    int col_extra = o.mirror_x ? ram_w - phys_cols : 0;
    if (row_extra < 0) row_extra = 0;
    if (col_extra < 0) col_extra = 0;
    // With MV (swap) set, CASET addresses physical rows -> extras land on the other axis.
    if (o.swap_xy) { o.gap_x += row_extra; o.gap_y += col_extra; }
    else           { o.gap_x += col_extra; o.gap_y += row_extra; }
    return o;
}
