#pragma once
#include <stdbool.h>
#include "esp_err.h"
#include "lvgl.h"
#include "animation_types.h"

typedef enum { ANIM_STATE_STOPPED, ANIM_STATE_PLAYING } animation_state_t;
typedef enum { ANIM_SRC_FALLBACK, ANIM_SRC_GIF } animation_source_t;

/*
 * All functions must run in LVGL context (an LVGL timer/event) or under lvgl_port_lock().
 *
 * animation_init() loads every available GIF once into its own lv_gif object (frame
 * buffers live in PSRAM). Switching only shows/hides + restarts, so repeated switching
 * allocates nothing. Missing/corrupt GIFs use the built-in drawn face.
 */
esp_err_t animation_init(lv_obj_t *parent);
esp_err_t animation_play(animation_id_t id);
esp_err_t animation_reload(animation_id_t id);
esp_err_t animation_stop(void);      // freeze current animation (stops decoding)
esp_err_t animation_resume(void);
esp_err_t animation_set_loop(bool enabled);   // applies to the next animation_play()
void animation_set_face_color(uint32_t rgb);  // built-in drawn face only

animation_state_t animation_get_state(void);
animation_id_t animation_current(void);
animation_source_t animation_source(animation_id_t id);
