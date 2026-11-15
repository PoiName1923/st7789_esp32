#pragma once
#include <stdbool.h>
#include <stdint.h>
#include "esp_err.h"

typedef struct {
    float temperature;      // degC, last known good
    float humidity;         // %RH, last known good
    bool valid;             // true if a good reading is newer than CONFIG_ROBOT_SENSOR_STALE_S
    int64_t timestamp_ms;   // uptime of last good reading, 0 = never
    uint32_t fail_count;    // consecutive failed reads
} sensor_data_t;

/** Start the DHT11 polling task (no-op if CONFIG_ROBOT_ENABLE_SENSOR is off). */
esp_err_t sensor_start(void);

/** Thread-safe snapshot of the latest reading. Never blocks. */
void sensor_get(sensor_data_t *out);
