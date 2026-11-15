#include "dht11_proto.h"

void dht11_bits_to_bytes(const uint16_t high_us[DHT11_BITS], uint8_t out[5])
{
    for (int i = 0; i < 5; i++) out[i] = 0;
    for (int i = 0; i < DHT11_BITS; i++)
        if (high_us[i] > DHT11_ONE_THRESHOLD_US) out[i / 8] |= (uint8_t)(0x80 >> (i % 8));
}

dht11_status_t dht11_decode(const uint8_t b[5], float *temp_c, float *rh)
{
    if ((b[0] | b[1] | b[2] | b[3] | b[4]) == 0) return DHT11_ERR_EMPTY;
    if ((uint8_t)(b[0] + b[1] + b[2] + b[3]) != b[4]) return DHT11_ERR_CHECKSUM;

    float h = b[0] + b[1] * 0.1f;
    float t = b[2] + (b[3] & 0x7F) * 0.1f;
    if (b[3] & 0x80) t = -t;          // newer DHT11 revisions flag negatives in bit 7
    // DHT11 spec: 0..50 C, 20..90 %RH. Allow some margin for clones; reject garbage.
    if (h < 0.0f || h > 100.0f || t < -20.0f || t > 60.0f) return DHT11_ERR_RANGE;

    *temp_c = t;
    *rh = h;
    return DHT11_OK;
}

const char *dht11_status_str(dht11_status_t s)
{
    switch (s) {
    case DHT11_OK:           return "ok";
    case DHT11_ERR_TIMEOUT:  return "timeout";
    case DHT11_ERR_CHECKSUM: return "checksum";
    case DHT11_ERR_RANGE:    return "out of range";
    case DHT11_ERR_EMPTY:    return "empty frame";
    }
    return "?";
}
