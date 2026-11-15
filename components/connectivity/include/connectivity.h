#pragma once
#include <stdbool.h>
#include <stdint.h>
#include "esp_err.h"
#include "connectivity_state.h"

typedef struct {
    connectivity_state_t state;
    char sta_ip[16];
    char ap_ip[16];
} connectivity_info_t;

/** Start Wi-Fi STA with auto-reconnect (exponential backoff). Requires nvs_flash_init(). */
esp_err_t connectivity_start(void);
wifi_status_t connectivity_status(void);
const char *connectivity_status_str(wifi_status_t s);
/** Block until connected or timeout. Returns true if connected. */
bool connectivity_wait(uint32_t timeout_ms);
esp_err_t connectivity_apply_settings(void);
void connectivity_get_info(connectivity_info_t *out);
