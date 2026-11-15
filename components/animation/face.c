#include "face.h"

#define EYE_GAP     50      // eye centre offset from screen centre
#define EYE_Y       (-8)
#define MORPH_MS    220
#define BLINK_MS    200
#define FLOAT_PX    4
#define FLOAT_MS    1600

typedef struct { int16_t w, h, dx, dy, radius; uint32_t color; } eye_shape_t;

static const eye_shape_t SHAPES[ANIMATION_COUNT] = {
    [ROBOT_IDLE]          = { 56, 72,   0,   0, 18, 0x00E5FF },
    [ROBOT_BLINK]         = { 56, 72,   0,   0, 18, 0x00E5FF },
    [ROBOT_HAPPY]         = { 64, 26,   0, -10, 13, 0x00E5FF },
    [ROBOT_SLEEPY]        = { 60, 10,   0,  18,  5, 0x0088AA },
    [ROBOT_SURPRISED]     = { 72, 88,   0,  -6, 36, 0x00E5FF },
    [ROBOT_LOOK_LEFT]     = { 50, 68, -28,   0, 16, 0x00E5FF },
    [ROBOT_LOOK_RIGHT]    = { 50, 68,  28,   0, 16, 0x00E5FF },
    [ROBOT_WEATHER_SUNNY] = { 64, 26,   0, -10, 13, 0xFFC800 },
    [ROBOT_WEATHER_RAIN]  = { 56, 40,   0,  10, 14, 0x4A90E2 },
};

typedef struct { lv_obj_t *obj; int32_t w, h, x, y, base_x; } eye_t;

static lv_obj_t *s_root;
static eye_t s_eye[2];
static int32_t s_blink_h;
static face_done_cb_t s_done_cb;
static uint32_t s_user_color;

static void set_w(void *v, int32_t x) { eye_t *e = v; e->w = x; lv_obj_set_width(e->obj, x); }
static void set_h(void *v, int32_t x) { eye_t *e = v; e->h = x; lv_obj_set_height(e->obj, x); }
static void set_x(void *v, int32_t x) { eye_t *e = v; e->x = x; lv_obj_set_x(e->obj, e->base_x + x); }
static void set_y(void *v, int32_t x) { eye_t *e = v; e->y = x; lv_obj_set_y(e->obj, EYE_Y + x); }
static void set_root_y(void *v, int32_t y) { lv_obj_set_y(v, y); }

static void anim_to(void *var, lv_anim_exec_xcb_t cb, int32_t from, int32_t to, uint32_t ms)
{
    lv_anim_delete(var, cb);
    if (from == to) { cb(var, to); return; }
    lv_anim_t a;
    lv_anim_init(&a);
    lv_anim_set_var(&a, var);
    lv_anim_set_exec_cb(&a, cb);
    lv_anim_set_values(&a, from, to);
    lv_anim_set_duration(&a, ms);
    lv_anim_set_path_cb(&a, lv_anim_path_ease_out);
    lv_anim_start(&a);
}

/* v: 0..1000 -> eyes close then reopen */
static void blink_cb(void *v, int32_t t)
{
    int32_t f = t < 500 ? 500 - t : t - 500;            // 500..0..500
    int32_t h = s_blink_h * f / 500;
    if (h < 4) h = 4;
    for (int i = 0; i < 2; i++) lv_obj_set_height(s_eye[i].obj, h);
}

static void blink_done(lv_anim_t *a)
{
    for (int i = 0; i < 2; i++) lv_obj_set_height(s_eye[i].obj, s_eye[i].h);
    if (s_done_cb) s_done_cb();
}

lv_obj_t *face_create(lv_obj_t *parent, face_done_cb_t on_oneshot_done)
{
    s_done_cb = on_oneshot_done;
    s_root = lv_obj_create(parent);
    lv_obj_remove_style_all(s_root);
    lv_obj_set_size(s_root, LV_PCT(100), LV_PCT(100));
    lv_obj_remove_flag(s_root, LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICKABLE);

    const eye_shape_t *s = &SHAPES[ROBOT_IDLE];
    for (int i = 0; i < 2; i++) {
        eye_t *e = &s_eye[i];
        e->obj = lv_obj_create(s_root);
        lv_obj_remove_style_all(e->obj);
        lv_obj_set_style_bg_opa(e->obj, LV_OPA_COVER, 0);
        lv_obj_set_style_bg_color(e->obj, lv_color_hex(s_user_color ? s_user_color : s->color), 0);
        lv_obj_set_style_radius(e->obj, s->radius, 0);
        e->base_x = i ? EYE_GAP : -EYE_GAP;
        e->w = s->w; e->h = s->h; e->x = 0; e->y = 0;
        lv_obj_set_size(e->obj, e->w, e->h);
        lv_obj_align(e->obj, LV_ALIGN_CENTER, e->base_x, EYE_Y);
    }
    return s_root;
}

void face_set_color(uint32_t rgb)
{
    s_user_color = rgb & 0xffffff;
    for (int i = 0; i < 2; i++) lv_obj_set_style_bg_color(s_eye[i].obj, lv_color_hex(s_user_color), 0);
}

void face_freeze(bool frozen)
{
    static bool s_floating;
    if (s_floating == !frozen) return;      // no restart -> no jump
    s_floating = !frozen;
    lv_anim_delete(s_root, set_root_y);
    lv_obj_set_y(s_root, 0);
    if (frozen) return;
    lv_anim_t a;
    lv_anim_init(&a);
    lv_anim_set_var(&a, s_root);
    lv_anim_set_exec_cb(&a, set_root_y);
    lv_anim_set_values(&a, -FLOAT_PX, FLOAT_PX);
    lv_anim_set_duration(&a, FLOAT_MS);
    lv_anim_set_reverse_duration(&a, FLOAT_MS);
    lv_anim_set_repeat_count(&a, LV_ANIM_REPEAT_INFINITE);
    lv_anim_set_path_cb(&a, lv_anim_path_ease_in_out);
    lv_anim_start(&a);
}

void face_show(animation_id_t id)
{
    if (id >= ANIMATION_COUNT) id = ROBOT_IDLE;
    const eye_shape_t *s = &SHAPES[id];
    lv_anim_delete(s_eye, blink_cb);
    for (int i = 0; i < 2; i++) {
        eye_t *e = &s_eye[i];
        lv_obj_set_style_bg_color(e->obj, lv_color_hex(s_user_color ? s_user_color : s->color), 0);
        lv_obj_set_style_radius(e->obj, s->radius, 0);
        anim_to(e, set_w, e->w, s->w, MORPH_MS);
        anim_to(e, set_h, e->h, s->h, MORPH_MS);
        anim_to(e, set_x, e->x, s->dx, MORPH_MS);
        anim_to(e, set_y, e->y, s->dy, MORPH_MS);
    }
    face_freeze(id != ROBOT_IDLE && id != ROBOT_BLINK);

    if (id == ROBOT_BLINK) {
        s_blink_h = s->h;
        lv_anim_t a;
        lv_anim_init(&a);
        lv_anim_set_var(&a, s_eye);
        lv_anim_set_exec_cb(&a, blink_cb);
        lv_anim_set_values(&a, 0, 1000);
        lv_anim_set_duration(&a, BLINK_MS);
        lv_anim_set_completed_cb(&a, blink_done);
        lv_anim_start(&a);
    }
}
