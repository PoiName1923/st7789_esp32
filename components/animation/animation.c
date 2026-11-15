#include <stdio.h>
#include <sys/stat.h>
#include "animation.h"
#include "face.h"
#include "gif_assets.h"
#include "esp_heap_caps.h"
#include "esp_log.h"
#if CONFIG_ROBOT_GIF_FROM_FS
#include "esp_spiffs.h"
#endif

#if LV_USE_GIF && (CONFIG_ROBOT_GIF_EMBEDDED || CONFIG_ROBOT_GIF_FROM_FS)
#define HAVE_GIF 1
#else
#define HAVE_GIF 0
#endif

static const char *TAG = "animation";
static lv_obj_t *s_face;
static lv_obj_t *s_gif[ANIMATION_COUNT];      // NULL -> built-in face for that id
static animation_id_t s_cur = ROBOT_IDLE;
static animation_state_t s_state = ANIM_STATE_STOPPED;
static bool s_loop = true, s_cur_loop = true, s_inited;

static void on_oneshot_done(void)
{
    if (!s_cur_loop) s_state = ANIM_STATE_STOPPED;
}

#if HAVE_GIF
static void gif_pause(lv_obj_t *g)  { lv_gif_pause(g); }
static void gif_resume(lv_obj_t *g) { lv_gif_resume(g); }
static void gif_start(lv_obj_t *g, bool loop)
{
    lv_gif_set_loop_count(g, loop ? 0 : 1);   // 0 = forever
    lv_gif_restart(g);
    lv_gif_resume(g);
}

static void gif_ready_cb(lv_event_t *e)
{
    // LV_EVENT_READY fires at the end of every pass; only matters for one-shots.
    if (lv_event_get_target(e) == s_gif[s_cur]) on_oneshot_done();
}

static lv_obj_t *load_gif(lv_obj_t *parent, animation_id_t id)
{
#if CONFIG_ROBOT_GIF_EMBEDDED
    if (id >= 9) return NULL;
    static lv_image_dsc_t dsc[ANIMATION_COUNT];        // must outlive the gif objects
    const gif_asset_t *a = gif_asset_find(g_gif_assets, g_gif_asset_count, animation_name(id));
    if (!a) return NULL;
    size_t len = (size_t)(a->end - a->start);
    if (!gif_header_valid(a->start, len)) {
        ESP_LOGW(TAG, "%s.gif is not a GIF file -> built-in face", animation_name(id));
        return NULL;
    }
    dsc[id] = (lv_image_dsc_t){
        .header = { .cf = LV_COLOR_FORMAT_RAW, .w = 0, .h = 0 },  // as in LVGL's GIF example
        .data_size = len,
        .data = a->start,
    };
    const void *src = &dsc[id];
#else
    static char lv_path[ANIMATION_COUNT][48];
    char path[40];
    struct stat st;
    if (id < 9) snprintf(path, sizeof(path), "/assets/%s.gif", animation_name(id));
    else snprintf(path, sizeof(path), "/assets/user_%02u.gif", (unsigned)id - 9);
    if (stat(path, &st) != 0) return NULL;
    snprintf(lv_path[id], sizeof(lv_path[id]), "S:%s", path);   // LVGL stdio driver letter 'S'
    const void *src = lv_path[id];
#endif
    size_t before = heap_caps_get_free_size(MALLOC_CAP_DEFAULT);
    lv_obj_t *g = lv_gif_create(parent);
    lv_gif_set_color_format(g, LV_COLOR_FORMAT_RGB565);   // before src: avoids the 4 B/px ARGB buffer
    lv_gif_set_src(g, src);
    if (!lv_gif_is_loaded(g)) {
        ESP_LOGW(TAG, "%s: corrupt or unsupported GIF -> built-in face", animation_name(id));
        lv_obj_delete(g);
        return NULL;
    }
    lv_gif_pause(g);
    lv_obj_add_flag(g, LV_OBJ_FLAG_HIDDEN);
    lv_obj_center(g);
    lv_obj_add_event_cb(g, gif_ready_cb, LV_EVENT_READY, NULL);
    ESP_LOGI(TAG, "%s: GIF loaded, %u KB", animation_name(id),
             (unsigned)((before - heap_caps_get_free_size(MALLOC_CAP_DEFAULT)) / 1024));
    return g;
}
#else
static void gif_pause(lv_obj_t *g) {}
static void gif_resume(lv_obj_t *g) {}
static void gif_start(lv_obj_t *g, bool loop) {}
#endif

static bool id_ok(animation_id_t id) { return (unsigned)id < ANIMATION_COUNT; }

esp_err_t animation_init(lv_obj_t *parent)
{
    if (s_inited) return ESP_OK;
#if CONFIG_ROBOT_GIF_FROM_FS
    esp_vfs_spiffs_conf_t conf = {
        .base_path = "/assets", .partition_label = "storage",
        .max_files = ANIMATION_COUNT + 1, .format_if_mount_failed = false,
    };
    esp_err_t err = esp_vfs_spiffs_register(&conf);
    if (err != ESP_OK) ESP_LOGW(TAG, "SPIFFS mount failed (%s): built-in face only", esp_err_to_name(err));
#endif
    s_face = face_create(parent, on_oneshot_done);
    int n = 0;
#if HAVE_GIF
    for (int id = 0; id < 9; id++) n += (s_gif[id] = load_gif(parent, id)) != NULL;
#endif
    ESP_LOGI(TAG, "%d/%d animations from GIF, the rest use the built-in face", n, ANIMATION_COUNT);
    s_inited = true;
    return animation_play(ROBOT_IDLE);
}

esp_err_t animation_play(animation_id_t id)
{
    if (!s_inited) return ESP_ERR_INVALID_STATE;
    if (!id_ok(id)) return ESP_ERR_INVALID_ARG;

    if (s_cur >= 9 && s_cur != id && s_gif[s_cur]) {
        lv_obj_delete(s_gif[s_cur]);
        s_gif[s_cur] = NULL;
    } else if (s_gif[s_cur]) {
        gif_pause(s_gif[s_cur]);
        lv_obj_add_flag(s_gif[s_cur], LV_OBJ_FLAG_HIDDEN);
    }
    if (id >= 9 && !s_gif[id]) s_gif[id] = load_gif(lv_obj_get_parent(s_face), id);
    s_cur = id;
    s_cur_loop = s_loop;
    s_state = ANIM_STATE_PLAYING;

    if (s_gif[id]) {
        face_freeze(true);
        lv_obj_add_flag(s_face, LV_OBJ_FLAG_HIDDEN);
        gif_start(s_gif[id], s_loop);
        lv_obj_remove_flag(s_gif[id], LV_OBJ_FLAG_HIDDEN);
    } else {
        lv_obj_remove_flag(s_face, LV_OBJ_FLAG_HIDDEN);
        face_show(id < 9 ? id : ROBOT_IDLE);
        // Drawn poses are static; a one-shot (except blink) is done once morphed.
        if (!s_loop && id != ROBOT_BLINK) s_state = ANIM_STATE_STOPPED;
    }
    return ESP_OK;
}

esp_err_t animation_reload(animation_id_t id)
{
    if (!s_inited || !id_ok(id)) return ESP_ERR_INVALID_ARG;
#if HAVE_GIF
    bool active = s_cur == id;
    if (s_gif[id]) {
        gif_pause(s_gif[id]);
        lv_obj_delete(s_gif[id]);
        s_gif[id] = NULL;
    }
    s_gif[id] = load_gif(lv_obj_get_parent(s_face), id);
    if (active) return animation_play(id);
#endif
    return ESP_OK;
}

esp_err_t animation_stop(void)
{
    if (!s_inited) return ESP_ERR_INVALID_STATE;
    if (s_gif[s_cur]) gif_pause(s_gif[s_cur]);
    else face_freeze(true);
    s_state = ANIM_STATE_STOPPED;
    return ESP_OK;
}

esp_err_t animation_resume(void)
{
    if (!s_inited) return ESP_ERR_INVALID_STATE;
    if (s_gif[s_cur]) gif_resume(s_gif[s_cur]);
    else face_freeze(s_cur != ROBOT_IDLE && s_cur != ROBOT_BLINK);
    s_state = ANIM_STATE_PLAYING;
    return ESP_OK;
}

esp_err_t animation_set_loop(bool enabled)
{
    s_loop = enabled;
    return ESP_OK;
}

void animation_set_face_color(uint32_t rgb) { face_set_color(rgb); }

animation_state_t animation_get_state(void) { return s_state; }
animation_id_t animation_current(void) { return s_cur; }

animation_source_t animation_source(animation_id_t id)
{
    return (id_ok(id) && s_gif[id]) ? ANIM_SRC_GIF : ANIM_SRC_FALLBACK;
}

