#pragma once
// Built-in fallback face drawn with LVGL objects (internal to the animation component).
#include "lvgl.h"
#include "animation.h"

typedef void (*face_done_cb_t)(void);

lv_obj_t *face_create(lv_obj_t *parent, face_done_cb_t on_oneshot_done);
void face_show(animation_id_t id);   // animate to the expression (BLINK = one-shot blink)
void face_freeze(bool frozen);       // stop/restart the idle "breathing" motion
void face_set_color(uint32_t rgb);
