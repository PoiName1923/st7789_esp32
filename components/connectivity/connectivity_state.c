#include "connectivity_state.h"

void connectivity_state_init(connectivity_state_t *s, bool has_sta_credentials)
{
    *s = (connectivity_state_t){ .sta = has_sta_credentials ? WIFI_STATUS_CONNECTING : WIFI_STATUS_DISABLED };
}

void connectivity_state_event(connectivity_state_t *s, connectivity_event_t event, int32_t error)
{
    if (!s) return;
    if (event == CONNECTIVITY_EVENT_STA_GOT_IP) { s->sta = WIFI_STATUS_CONNECTED; s->last_error = 0; }
    else { s->sta = WIFI_STATUS_CONNECTING; s->last_error = error; }
    s->ap_ready = false;
}
