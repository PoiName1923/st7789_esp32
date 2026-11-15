#include "web_auth.h"
#include <string.h>

static bool due(uint32_t now, uint32_t then) { return (int32_t)(now - then) >= 0; }
static uint32_t next(uint32_t *x) { *x ^= *x << 13; *x ^= *x >> 17; *x ^= *x << 5; return *x; }

static void hex(char *out, size_t bytes, uint32_t *seed)
{
    static const char digits[] = "0123456789abcdef";
    for (size_t i = 0; i < bytes * 2; i++) out[i] = digits[next(seed) & 15];
    out[bytes * 2] = '\0';
}

static bool constant_equal(const char *a, const char *b, size_t n)
{
    size_t alen = 0, blen = 0;
    while (alen <= n && a[alen]) alen++;
    while (blen <= n && b[blen]) blen++;
    if (alen != n || blen != n) return false;
    unsigned diff = 0;
    for (size_t i = 0; i < n; i++) diff |= (unsigned char)a[i] ^ (unsigned char)b[i];
    return diff == 0;
}

void web_auth_init(web_auth_t *auth) { memset(auth, 0, sizeof(*auth)); }

web_session_t *web_auth_create(web_auth_t *auth, uint32_t now, uint32_t seed)
{
    web_session_t *slot = NULL;
    for (size_t i = 0; i < WEB_MAX_SESSIONS; i++)
        if (!auth->sessions[i].active || due(now, auth->sessions[i].expires_ms)) { slot = &auth->sessions[i]; break; }
    if (!slot) slot = &auth->sessions[0];
    if (!seed) seed = 0x9e3779b9u;
    hex(slot->token, 16, &seed); hex(slot->csrf, 8, &seed);
    slot->expires_ms = now + WEB_SESSION_TTL_MS; slot->active = true;
    return slot;
}

web_session_t *web_auth_find(web_auth_t *auth, const char *token, uint32_t now)
{
    if (!auth || !token) return NULL;
    for (size_t i = 0; i < WEB_MAX_SESSIONS; i++) {
        web_session_t *s = &auth->sessions[i];
        if (s->active && due(now, s->expires_ms)) s->active = false;
        if (s->active && constant_equal(s->token, token, 32)) return s;
    }
    return NULL;
}

bool web_auth_authorize(web_auth_t *auth, const char *token, const char *csrf, uint32_t now)
{
    web_session_t *s = web_auth_find(auth, token, now);
    return s && csrf && constant_equal(s->csrf, csrf, 16);
}

void web_auth_invalidate_all(web_auth_t *auth)
{
    for (size_t i = 0; i < WEB_MAX_SESSIONS; i++) auth->sessions[i].active = false;
}

bool web_auth_login_allowed(web_auth_t *auth, uint32_t now)
{
    return !auth->blocked_until_ms || due(now, auth->blocked_until_ms);
}

void web_auth_login_failed(web_auth_t *auth, uint32_t now)
{
    if (++auth->failures >= 3) { auth->blocked_until_ms = now + WEB_LOGIN_BLOCK_MS; auth->failures = 0; }
}

void web_auth_login_succeeded(web_auth_t *auth) { auth->failures = 0; auth->blocked_until_ms = 0; }
