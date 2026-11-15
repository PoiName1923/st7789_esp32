#include <string.h>
#include "gif_assets.h"

const gif_asset_t *gif_asset_find(const gif_asset_t *table, unsigned n, const char *name)
{
    for (unsigned i = 0; i < n; i++)
        if (table[i].name && strcmp(table[i].name, name) == 0) return &table[i];
    return NULL;
}

bool gif_header_valid(const uint8_t *data, size_t len)
{
    // 6-byte signature + 7-byte logical screen descriptor + trailer
    return data && len >= 14 &&
           (memcmp(data, "GIF87a", 6) == 0 || memcmp(data, "GIF89a", 6) == 0);
}
