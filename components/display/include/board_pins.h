#pragma once
// Pure logic (no ESP-IDF deps) -> host-testable. ESP32-S3 N16R8 pin rules.
#include <stddef.h>

typedef enum {
    PIN_OK = 0,
    PIN_UNUSED,             // -1: not connected (only fine for optional pins)
    PIN_WARN_STRAPPING,     // GPIO0/3/45/46: affect boot mode
    PIN_WARN_USB,           // GPIO19/20: native USB D-/D+
    PIN_WARN_UART0,         // GPIO43/44: console UART
    PIN_ERR_INVALID,        // does not exist on ESP32-S3
    PIN_ERR_FLASH_PSRAM,    // GPIO26-37: octal flash/PSRAM bus on N16R8
} pin_status_t;

pin_status_t board_pin_classify(int gpio);
const char *board_pin_status_str(pin_status_t s);
/** Returns the first GPIO (>=0) used twice in the list, or -1. */
int board_pins_find_duplicate(const int *gpios, size_t n);
