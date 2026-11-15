#pragma once
// Pure (host-testable). Exponential backoff: base * 2^attempt, capped at max.
#include <stdint.h>

static inline uint32_t backoff_delay_ms(uint32_t attempt, uint32_t base_ms, uint32_t max_ms)
{
    if (attempt >= 31 || base_ms > (max_ms >> attempt)) return max_ms;
    return base_ms << attempt;
}
