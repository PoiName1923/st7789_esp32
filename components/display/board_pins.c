#include "board_pins.h"

pin_status_t board_pin_classify(int gpio)
{
    if (gpio == -1) return PIN_UNUSED;
    if (gpio < 0 || gpio > 48 || (gpio >= 22 && gpio <= 25)) return PIN_ERR_INVALID;
    if (gpio >= 26 && gpio <= 37) return PIN_ERR_FLASH_PSRAM;
    if (gpio == 0 || gpio == 3 || gpio == 45 || gpio == 46) return PIN_WARN_STRAPPING;
    if (gpio == 19 || gpio == 20) return PIN_WARN_USB;
    if (gpio == 43 || gpio == 44) return PIN_WARN_UART0;
    return PIN_OK;
}

const char *board_pin_status_str(pin_status_t s)
{
    switch (s) {
    case PIN_OK:              return "ok";
    case PIN_UNUSED:          return "not connected";
    case PIN_WARN_STRAPPING:  return "strapping pin (may break boot)";
    case PIN_WARN_USB:        return "native USB pin (USB-JTAG/console lost)";
    case PIN_WARN_UART0:      return "UART0 console pin";
    case PIN_ERR_INVALID:     return "not a valid ESP32-S3 GPIO";
    case PIN_ERR_FLASH_PSRAM: return "reserved for octal flash/PSRAM on N16R8";
    }
    return "?";
}

int board_pins_find_duplicate(const int *gpios, size_t n)
{
    for (size_t i = 0; i < n; i++) {
        if (gpios[i] < 0) continue;
        for (size_t j = i + 1; j < n; j++)
            if (gpios[i] == gpios[j]) return gpios[i];
    }
    return -1;
}
