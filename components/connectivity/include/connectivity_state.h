#pragma once
#include <stdbool.h>
#include <stdint.h>

typedef enum { WIFI_STATUS_DISABLED, WIFI_STATUS_CONNECTING, WIFI_STATUS_CONNECTED } wifi_status_t;
typedef enum {
    CONNECTIVITY_EVENT_STA_GOT_IP,
    CONNECTIVITY_EVENT_STA_DISCONNECTED,
    CONNECTIVITY_EVENT_CREDENTIALS_CHANGED,
} connectivity_event_t;
typedef struct { wifi_status_t sta; bool ap_ready; int32_t last_error; } connectivity_state_t;

void connectivity_state_init(connectivity_state_t *state, bool has_sta_credentials);
void connectivity_state_event(connectivity_state_t *state, connectivity_event_t event, int32_t error);
