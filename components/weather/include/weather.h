#pragma once
#include <stdbool.h>
#include <stdint.h>
#include "esp_err.h"
#include "weather_parse.h"

typedef struct {
    weather_current_t cur;  // last successful result (kept while offline)
    bool valid;             // at least one successful fetch
    bool stale;             // last success older than 2x update interval
    int64_t updated_ms;     // uptime of last success
} weather_data_t;

/** Start the Open-Meteo fetch task (no-op if weather disabled or no SSID). */
esp_err_t weather_start(void);
/** Thread-safe snapshot. Never blocks. */
void weather_get(weather_data_t *out);
