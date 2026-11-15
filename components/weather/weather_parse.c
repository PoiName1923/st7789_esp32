#include <ctype.h>
#include <stdlib.h>
#include <string.h>
#include "weather_parse.h"

/* ponytail: tiny key scanner instead of cJSON. Open-Meteo's "current" object is flat
 * (numbers + one string), which is all this handles. Switch to cJSON if nesting is needed. */

static const char *skip_ws(const char *p, const char *end)
{
    while (p < end && isspace((unsigned char)*p)) p++;
    return p;
}

/* Returns pointer to the value of "key" within [p, end), or NULL. */
static const char *find_value(const char *p, const char *end, const char *key)
{
    size_t klen = strlen(key);
    for (; p + klen + 2 <= end; p++) {
        if (p[0] != '"' || strncmp(p + 1, key, klen) != 0 || p[klen + 1] != '"') continue;
        const char *v = skip_ws(p + klen + 2, end);
        if (v < end && *v == ':') return skip_ws(v + 1, end);
    }
    return NULL;
}

static int read_num(const char *start, const char *end, const char *key, double *out)
{
    const char *v = find_value(start, end, key);
    if (!v) return -1;
    char *stop;
    *out = strtod(v, &stop);
    return (stop == v || stop > end) ? -1 : 0;
}

int weather_parse_open_meteo(const char *json, weather_current_t *out)
{
    if (!json) return -1;
    const char *jend = json + strlen(json);
    const char *obj = find_value(json, jend, "current");
    if (!obj || *obj != '{') return -1;

    // Find the matching '}' (string-aware, tolerates nesting).
    const char *end = obj;
    int depth = 0;
    bool in_str = false;
    for (; end < jend; end++) {
        if (in_str) { if (*end == '\\') end++; else if (*end == '"') in_str = false; continue; }
        if (*end == '"') in_str = true;
        else if (*end == '{') depth++;
        else if (*end == '}' && --depth == 0) break;
    }
    if (end >= jend) return -1;

    double t, h, code, day, wind;
    if (read_num(obj, end, "temperature_2m", &t) || read_num(obj, end, "relative_humidity_2m", &h) ||
        read_num(obj, end, "weather_code", &code) || read_num(obj, end, "is_day", &day) ||
        read_num(obj, end, "wind_speed_10m", &wind))
        return -1;

    weather_current_t r = {
        .temperature = (float)t, .humidity = (float)h, .weather_code = (int)code,
        .is_day = day != 0, .wind_speed = (float)wind,
    };
    const char *tv = find_value(obj, end, "time");
    if (tv && *tv == '"') {
        const char *q = memchr(tv + 1, '"', (size_t)(end - tv - 1));
        size_t n = q ? (size_t)(q - tv - 1) : 0;
        if (n >= sizeof(r.time)) n = sizeof(r.time) - 1;
        memcpy(r.time, tv + 1, n);
        r.time[n] = '\0';
    }
    *out = r;
    return 0;
}

/* WMO weather interpretation codes, per https://open-meteo.com/en/docs */
wx_category_t weather_code_category(int c)
{
    switch (c) {
    case 0: case 1:                         return WX_CLEAR;
    case 2:                                 return WX_PARTLY_CLOUDY;
    case 3:                                 return WX_CLOUDY;
    case 45: case 48:                       return WX_FOG;
    case 51: case 53: case 55: case 56: case 57: return WX_DRIZZLE;
    case 61: case 63: case 65: case 66: case 67:
    case 80: case 81: case 82:              return WX_RAIN;
    case 71: case 73: case 75: case 77: case 85: case 86: return WX_SNOW;
    case 95: case 96: case 99:              return WX_THUNDER;
    default:                                return WX_UNKNOWN;
    }
}

const char *weather_code_text(int c)
{
    switch (c) {
    case 0:  return "Clear sky";
    case 1:  return "Mainly clear";
    case 2:  return "Partly cloudy";
    case 3:  return "Overcast";
    case 45: case 48: return "Fog";
    case 51: case 53: case 55: return "Drizzle";
    case 56: case 57: return "Freezing drizzle";
    case 61: return "Light rain";
    case 63: return "Rain";
    case 65: return "Heavy rain";
    case 66: case 67: return "Freezing rain";
    case 71: case 73: case 75: return "Snow";
    case 77: return "Snow grains";
    case 80: case 81: return "Rain showers";
    case 82: return "Violent showers";
    case 85: case 86: return "Snow showers";
    case 95: return "Thunderstorm";
    case 96: case 99: return "Storm + hail";
    default: return "Unknown";
    }
}
