#pragma once
// Pure (no ESP-IDF deps) -> host-testable.
#include <stdbool.h>

typedef enum {
    WX_CLEAR, WX_PARTLY_CLOUDY, WX_CLOUDY, WX_FOG, WX_DRIZZLE,
    WX_RAIN, WX_SNOW, WX_THUNDER, WX_UNKNOWN,
} wx_category_t;

typedef struct {
    float temperature;    // degC
    float humidity;       // %RH
    int weather_code;     // WMO code
    bool is_day;
    float wind_speed;     // km/h
    char time[20];        // local time from API, "YYYY-MM-DDTHH:MM"
} weather_current_t;

/**
 * Parse the "current" object of an Open-Meteo /v1/forecast response requested with
 * current=temperature_2m,relative_humidity_2m,weather_code,is_day,wind_speed_10m
 * Returns 0 on success, -1 if "current" or any field is missing/malformed (out untouched).
 */
int weather_parse_open_meteo(const char *json, weather_current_t *out);

wx_category_t weather_code_category(int wmo_code);
const char *weather_code_text(int wmo_code);
