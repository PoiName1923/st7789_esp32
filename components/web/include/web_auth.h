#pragma once
#include <stdbool.h>
#include <stdint.h>

#define WEB_MAX_SESSIONS 4
#define WEB_SESSION_TTL_MS (30u * 60u * 1000u)
#define WEB_LOGIN_BLOCK_MS 60000u

typedef struct {
    char token[33];
    char csrf[17];
    uint32_t expires_ms;
    bool active;
} web_session_t;

typedef struct {
    web_session_t sessions[WEB_MAX_SESSIONS];
    uint8_t failures;
    uint32_t blocked_until_ms;
} web_auth_t;

void web_auth_init(web_auth_t *auth);
web_session_t *web_auth_create(web_auth_t *auth, uint32_t now_ms, uint32_t seed);
web_session_t *web_auth_find(web_auth_t *auth, const char *token, uint32_t now_ms);
bool web_auth_authorize(web_auth_t *auth, const char *token, const char *csrf, uint32_t now_ms);
void web_auth_invalidate_all(web_auth_t *auth);
bool web_auth_login_allowed(web_auth_t *auth, uint32_t now_ms);
void web_auth_login_failed(web_auth_t *auth, uint32_t now_ms);
void web_auth_login_succeeded(web_auth_t *auth);
