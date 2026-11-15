#pragma once
// Pure DHT11 frame logic (no ESP-IDF deps) -> host-testable.
#include <stdint.h>

#define DHT11_BITS 40
#define DHT11_ONE_THRESHOLD_US 40   // '0' high ~26-28 us, '1' high ~70 us

typedef enum {
    DHT11_OK = 0,
    DHT11_ERR_TIMEOUT = -1,
    DHT11_ERR_CHECKSUM = -2,
    DHT11_ERR_RANGE = -3,
    DHT11_ERR_EMPTY = -4,     // all-zero frame (line stuck / no sensor)
} dht11_status_t;

/** Pack 40 measured high-pulse widths into 5 bytes (MSB first). */
void dht11_bits_to_bytes(const uint16_t high_us[DHT11_BITS], uint8_t out[5]);

/** Validate checksum + plausible range and decode. Outputs untouched on error. */
dht11_status_t dht11_decode(const uint8_t b[5], float *temp_c, float *rh);

const char *dht11_status_str(dht11_status_t s);
