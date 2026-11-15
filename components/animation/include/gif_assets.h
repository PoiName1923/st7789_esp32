#pragma once
// Pure (no ESP-IDF deps) -> host-testable.
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef struct {
    const char *name;        // e.g. "robot_idle" (file assets/gifs/robot_idle.gif)
    const uint8_t *start;
    const uint8_t *end;
} gif_asset_t;

/* Generated at configure time from whatever exists in assets/gifs (see CMakeLists.txt). */
extern const gif_asset_t g_gif_assets[];
extern const unsigned g_gif_asset_count;

const gif_asset_t *gif_asset_find(const gif_asset_t *table, unsigned n, const char *name);
/** Cheap sanity check before handing bytes to the decoder: GIF87a/GIF89a magic + minimum size. */
bool gif_header_valid(const uint8_t *data, size_t len);
