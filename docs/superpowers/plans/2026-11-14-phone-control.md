# Phone Control Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Add a secure, phone-friendly local web control panel with persistent runtime settings and fallback Wi-Fi for the ESP32-S3 robot.

**Architecture:** A `settings` component owns validation, defaults, password derivation, and NVS persistence. `connectivity` consumes runtime network settings and maintains concurrent STA/AP fallback service; a self-contained `web` component serves embedded HTTPS assets, authenticates sessions, validates state-changing requests, and forwards UI commands through an `app_controller` queue so only the LVGL context mutates the robot UI.

**Tech Stack:** ESP-IDF C, NVS, esp_wifi/esp_netif, esp_https_server, mbedTLS, FreeRTOS queues, embedded HTML/CSS/JavaScript, existing host-test runner.

**Spec:** `docs/superpowers/specs/2026-11-14-phone-control-design.md`

## Global Constraints

- No cloud, native phone app, OTA, or uploaded GIF support.
- Do not log passwords or return them from any API.
- Admin and fallback AP passwords are independent; first-build values come from menuconfig and have no shared default.
- Fallback AP passwords must be ASCII and 8–63 characters.
- The web UI is HTTPS only and uses a device certificate; credentials never travel over plaintext HTTP.
- Admin passwords are stored as a salted password-derived hash; recoverable Wi-Fi credentials require flash/NVS encryption for physical-extraction resistance.
- Only the LVGL task may mutate UI state.
- Backlight brightness is unavailable when `CONFIG_ROBOT_PIN_LCD_BL == -1`.

## Review Focus

- Corrupt or partially missing NVS records must fall back to valid build defaults without preventing boot; covered by Task 1 host tests.
- Empty/open home Wi-Fi and maximum-length SSID/password values must validate and round-trip without truncation; covered by Task 1 host tests.
- Expired, malformed, or invalidated sessions must not authorize any state-changing endpoint; covered by Task 4 host tests.
- A full command queue must return a clear API error and must never call LVGL directly from the HTTP task; covered by Task 3 host tests.
- STA failure or credential change must leave the fallback AP reachable and report reconnect guidance; covered by Task 2 state tests and Task 5 API/UI checks.

---

### Task 1: Runtime settings and persistence

**Files:**
- Create: `components/settings/include/robot_settings.h`
- Create: `components/settings/settings_validate.c`
- Create: `components/settings/settings.c`
- Create: `components/settings/CMakeLists.txt`
- Modify: `main/Kconfig.projbuild`
- Modify: `main/CMakeLists.txt`
- Modify: `main/app_main.c`
- Modify: `tests/host/test_main.c`
- Modify: `tests/host/run.sh`

**Interfaces:**
- Consumes: NVS initialized by `app_main`; existing build-time display, animation, weather, and Wi-Fi defaults.
- Produces: `esp_err_t settings_init(void)`, `void settings_get(robot_settings_t *out)`, `esp_err_t settings_update(const robot_settings_patch_t *patch, settings_change_t *change)`, `bool settings_verify_admin(const char *password)`, and `esp_err_t settings_change_admin(const char *old_password, const char *new_password)`.

- [ ] **Step 1: Add failing host tests for defaults and validation**

  Extend `test_main.c` with assertions for screen mode, animation id/FPS, face color, screen durations, display rotation/order/invert/offsets, Wi-Fi credentials, weather location/timezone/interval, AP password ASCII length, and invalid enum/range/string inputs. Assert empty home Wi-Fi password is accepted and AP passwords outside 8–63 ASCII bytes are rejected.

- [ ] **Step 2: Run host tests and verify they fail**

  Run: `sh tests/host/run.sh`

  Expected: compile failure because `robot_settings.h` and validation functions do not exist.

- [ ] **Step 3: Implement the settings data model and pure validation**

  Define fixed-size structs and `settings_defaults(robot_settings_t *)`, `settings_validate(const robot_settings_t *)`, and `settings_apply_patch(...)` in `settings_validate.c`; preserve secrets when omitted from a patch and expose capability/restart flags separately from stored values.

- [ ] **Step 4: Run host tests and verify validation passes**

  Run: `sh tests/host/run.sh`

  Expected: all host checks pass.

- [ ] **Step 5: Add NVS persistence and password derivation**

  Implement one versioned settings blob with validation-before-save, defaults/migration-on-load, per-password random salt, PBKDF2-HMAC-SHA256 admin verification, and secret-safe logging. Add menuconfig entries for initial admin password, fallback AP SSID/password, and HTTPS certificate/key PEM inputs; require non-empty independent initial passwords.

- [ ] **Step 6: Initialize settings before hardware consumers**

  Call `settings_init()` after NVS initialization in `app_main`; add component dependencies.

- [ ] **Step 7: Build firmware**

  Run: `idf.py build`

  Expected: build succeeds with no new warnings.

### Task 2: Runtime Wi-Fi with fallback access point

**Files:**
- Create: `components/connectivity/include/connectivity_state.h`
- Create: `components/connectivity/connectivity_state.c`
- Modify: `components/connectivity/include/connectivity.h`
- Modify: `components/connectivity/connectivity.c`
- Modify: `components/connectivity/CMakeLists.txt`
- Modify: `tests/host/test_main.c`
- Modify: `tests/host/run.sh`

**Interfaces:**
- Consumes: `settings_get(robot_settings_t *)` and validated home/AP Wi-Fi settings.
- Produces: existing status API plus `esp_err_t connectivity_apply_settings(void)`, `void connectivity_get_info(connectivity_info_t *out)`, and a pure `connectivity_state_event(...)` transition helper.

- [ ] **Step 1: Add failing connectivity state tests**

  Assert AP is enabled without STA credentials, remains enabled during STA retries/failure, reports STA IP after connection, and preserves AP availability across credential reconfiguration.

- [ ] **Step 2: Run host tests and verify they fail**

  Run: `sh tests/host/run.sh`

  Expected: compile failure because the state helper is absent.

- [ ] **Step 3: Implement the minimal pure state helper**

  Add explicit disabled/connecting/connected STA state plus AP-ready and last-error fields without ESP-IDF dependencies.

- [ ] **Step 4: Implement APSTA connectivity using runtime settings**

  Replace direct `CONFIG_ROBOT_WIFI_*` reads with settings snapshots, create default STA and AP netifs, configure WPA2/WPA3-compatible AP credentials, retain exponential retry, and apply changed credentials through `connectivity_apply_settings()` without logging secrets.

- [ ] **Step 5: Run host tests and firmware build**

  Run: `sh tests/host/run.sh`

  Run: `idf.py build`

  Expected: both succeed.

### Task 3: Command queue and runtime robot behavior

**Files:**
- Create: `components/app_controller/include/robot_command.h`
- Create: `components/app_controller/robot_command.c`
- Modify: `components/app_controller/include/app_controller.h`
- Modify: `components/app_controller/app_controller.c`
- Modify: `components/app_controller/CMakeLists.txt`
- Modify: `components/weather/weather.c`
- Modify: `tests/host/test_main.c`
- Modify: `tests/host/run.sh`

**Interfaces:**
- Consumes: runtime settings and existing animation/UI/weather functions.
- Produces: `bool app_controller_send(const robot_command_t *command, uint32_t timeout_ms)` and pure `robot_command_validate(const robot_command_t *)`.

- [ ] **Step 1: Add failing command validation tests**

  Cover valid screen/animation choices, invalid enum IDs, invalid colors/FPS/durations, and the restart-required display setting command.

- [ ] **Step 2: Run host tests and verify they fail**

  Run: `sh tests/host/run.sh`

  Expected: compile failure because command types/functions are absent.

- [ ] **Step 3: Implement command model and queue**

  Define the smallest tagged command union needed by the spec, create a bounded FreeRTOS queue in `app_controller_start`, and drain it in `tick_cb`; return `false` when the queue is full.

- [ ] **Step 4: Apply commands only in LVGL context**

  Add manual/automatic screen mode, explicit embedded animation selection, runtime FPS/face-color/timing updates, and settings persistence. Mark display rotation/order/invert/offset changes as saved but restart-required. Keep brightness read-only unsupported for `PIN_LCD_BL == -1`.

- [ ] **Step 5: Read weather location/timezone/interval at runtime**

  Snapshot settings before each request/delay so changes take effect without rebuilding; never expose stored Wi-Fi secrets.

- [ ] **Step 6: Run host tests and firmware build**

  Run: `sh tests/host/run.sh`

  Run: `idf.py build`

  Expected: both succeed.

### Task 4: Authentication and HTTPS server

**Files:**
- Create: `components/web/include/web_server.h`
- Create: `components/web/auth.h`
- Create: `components/web/auth.c`
- Create: `components/web/web_server.c`
- Create: `components/web/CMakeLists.txt`
- Modify: `main/CMakeLists.txt`
- Modify: `main/app_main.c`
- Modify: `tests/host/test_main.c`
- Modify: `tests/host/run.sh`

**Interfaces:**
- Consumes: settings password verification/change, connectivity status, sensor/weather snapshots, and `app_controller_send`.
- Produces: `esp_err_t web_server_start(void)` and pure bounded session/rate-limit helpers used by request handlers.

- [ ] **Step 1: Add failing authentication/session tests**

  Assert login throttling, cryptographically random opaque token lookup, expiry, logout, global invalidation after password change, constant-time token comparison, CSRF rejection, and bounded session eviction.

- [ ] **Step 2: Run host tests and verify they fail**

  Run: `sh tests/host/run.sh`

  Expected: compile failure because auth helpers are absent.

- [ ] **Step 3: Implement bounded auth state**

  Use fixed-size session and failure tables, `esp_fill_random` tokens, inactivity/absolute expiry, `Secure; HttpOnly; SameSite=Strict` cookies, Origin plus per-session CSRF validation on mutations, and invalidation after admin password changes.

- [ ] **Step 4: Implement HTTPS handlers**

  Serve only TLS using the configured certificate/key. Add login/logout, sanitized status/settings GET responses, validated settings/command/password PUT/POST handlers, consistent JSON errors, body-size limits, and `503` for a full command queue.

- [ ] **Step 5: Start HTTPS after connectivity and controller initialization**

  Wire `web_server_start()` in `app_main` without making robot animation depend on web startup success.

- [ ] **Step 6: Run host tests and firmware build**

  Run: `sh tests/host/run.sh`

  Run: `idf.py build`

  Expected: both succeed.

### Task 5: Embedded responsive phone UI

**Files:**
- Create: `components/web/assets/index.html`
- Create: `components/web/assets/app.css`
- Create: `components/web/assets/app.js`
- Modify: `components/web/CMakeLists.txt`
- Modify: `components/web/web_server.c`

**Interfaces:**
- Consumes: Task 4 JSON endpoints.
- Produces: embedded login and control UI served by HTTPS.

- [ ] **Step 1: Add an asset/API smoke check**

  Add a host-side script or C test that asserts the HTML references only embedded assets, contains the login/control/status sections, and JavaScript does not render or request stored password values.

- [ ] **Step 2: Run the smoke check and verify it fails**

  Run: `sh tests/host/run.sh`

  Expected: failure because assets are absent.

- [ ] **Step 3: Build the mobile-first control UI**

  Implement login, screen/expression/animation controls, FPS/color/timing settings, display settings with restart notice, disabled brightness with the 3V3 wiring explanation, network/weather forms, sensor/network/error status, and separate password-change forms. Warn before applying Wi-Fi/AP changes and show reconnection instructions afterward.

- [ ] **Step 4: Embed assets without external dependencies**

  Use `EMBED_TXTFILES` and serve HTML/CSS/JS with correct content types and cache rules; no CDN or runtime Internet dependency.

- [ ] **Step 5: Run all automated checks**

  Run: `sh tests/host/run.sh`

  Run: `idf.py build`

  Expected: both succeed.

### Task 6: Documentation and end-to-end verification

**Files:**
- Modify: `README.md`
- Modify: `HUONG_DAN.md`
- Modify: `sdkconfig.defaults`

**Interfaces:**
- Consumes: completed Tasks 1–5.
- Produces: reproducible setup and a split host/on-board verification checklist.

- [ ] **Step 1: Document secure first-time configuration**

  Explain independent initial passwords, certificate/key provisioning, self-signed certificate acceptance on phones, AP discovery/reconnection, and optional NVS/flash encryption for physical security.

- [ ] **Step 2: Document capabilities and restart behavior**

  List runtime settings, display settings requiring restart, the unsupported brightness case for BL tied to 3V3, and the explicit non-goals.

- [ ] **Step 3: Run host regression tests**

  Run: `sh tests/host/run.sh`

  Expected: all checks pass.

- [ ] **Step 4: Build the production firmware configuration**

  Run: `idf.py build`

  Expected: build succeeds.

- [ ] **Step 5: Record hardware-only checks**

  Leave a checklist for phone login, each setting, reset persistence, bad home Wi-Fi fallback, AP credential reconnection, certificate flow, and old-session invalidation after password change; do not claim these passed without a board.
