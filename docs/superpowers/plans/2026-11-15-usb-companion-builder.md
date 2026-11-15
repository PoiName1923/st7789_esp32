# USB Companion Implementation Plan

> **For agentic workers:** Use superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Deliver a Windows companion that controls the ESP32-S3 over USB, manages persistent GIFs, and builds/flashes firmware from its UI.

**Architecture:** Firmware exposes a versioned serial protocol and persists one selected screen. The Windows app hosts the existing web UI on loopback, manages a mirrored GIF library, and invokes bundled portable build/flash tools.

**Tech Stack:** ESP-IDF 5.5, C, LVGL, SPIFFS, .NET 8 WPF, Windows 10/11 x64.

**Spec:** `docs/superpowers/specs/2026-11-15-usb-companion-builder-design.md`

## Global Constraints

- Bot is authoritative for GIFs; verify a complete backup before full flash.
- Native USB is the runtime transport; UART bridge is a recovery flash path.
- Never mutate LVGL outside its task.
- Weather may use Wi-Fi STA; USB handles control.
- GIF changes do not rebuild the application firmware.

## Review Focus

- USB disconnect must retain the selected screen and report a failed command.
- Corrupt GIF metadata must fall back to the built-in face.
- Full flash must stop before erase when GIF backup verification fails.
- A new PC must retrieve the bot's GIFs before replacing storage.
- A localhost request without the session token must not control the bot.

---

### Task 1: Persistent single-screen behavior

**Files:** `components/app_controller/app_controller.c`, `components/settings/include/robot_settings.h`, `components/settings/settings_validate.c`, `tests/host/test_main.c`

**Interfaces:** Existing `robot_settings_t`, `app_controller_send`, and LVGL command queue.

- [ ] Add host checks for no automatic transition and persistence/fallback of the selected screen.
- [ ] Run `sh tests/host/run.sh` and confirm expected failure.
- [ ] Change controller startup/tick and settings validation to preserve one screen until a command.
- [ ] Run host tests and firmware build.

### Task 2: Versioned USB control protocol

**Files:** create `components/usb_control/` with protocol parser, transport task and CMake; modify `main/app_main.c`, `main/CMakeLists.txt`, host tests.

**Interfaces:** `usb_control_start()`, versioned `hello/get_status/get_settings/list_screens/show_screen/show_gif/set_settings`, bounded request IDs and responses.

- [ ] Test malformed/oversize frames, matching IDs and queue-full behavior.
- [ ] Implement framed serial I/O and dispatch validated mutations via `app_controller_send`.
- [ ] Run host tests and ESP-IDF build; check on native USB hardware.

### Task 3: GIF manifest and on-demand playback

**Files:** `components/animation/animation.c`, new manifest reader, `partitions.csv`, host tests.

**Interfaces:** list/select GIF by stable ID; chunked read for backup; checksum-verified SPIFFS files.

- [ ] Test corrupt manifest, missing GIF, duplicate ID, checksum mismatch and fallback.
- [ ] Load only the active GIF decoder, mount storage without formatting, and expose chunked reads through Task 2.
- [ ] Run host tests and ESP-IDF build; measure PSRAM on hardware.

### Task 4: Windows companion and localhost UI

**Files:** create `companion/` .NET 8 WPF project, USB client, loopback server, UI assets and tests.

**Interfaces:** serial discovery/handshake; authenticated loopback API; four UI areas: Connection, Control, GIF Library, Firmware.

- [ ] Test request timeout/disconnect and localhost token enforcement using a fake serial device.
- [ ] Implement native USB discovery, reconnect by chip ID and the HTTP bridge.
- [ ] Adapt existing web assets for screen and GIF selection; verify instant acknowledgement.
- [ ] Run `dotnet test` and publish self-contained Windows app.

### Task 5: GIF import and safe storage transfer

**Files:** companion GIF service and UI, `tools/convert_gif.py`, firmware USB storage transfer handlers, tests.

**Interfaces:** import/preview/list/remove/rename; read bot manifest and GIF chunks; write verified storage image.

- [ ] Test fresh-PC backup, size limit, duplicate names, checksum failure and interrupted transfer.
- [ ] Implement conversion, cache by chip ID, verified backup and SPIFFS image creation.
- [ ] Test adding GIF without rebuilding firmware and fallback after deletion on hardware.

### Task 6: Build, flash and installer

**Files:** companion firmware service/UI, packaging scripts, documentation and release metadata.

**Interfaces:** prebuilt flash and build-then-flash; validated board options; portable ESP-IDF and tools.

- [ ] Test that backup failure prevents erase, build failure prevents flash and reconnect verifies version.
- [ ] Bundle matched source, binaries, portable toolchain and SPIFFS tools in a Windows installer.
- [ ] Run clean Windows 10/11 x64 installation, build, flash, reconnect and library-restore hardware checks.

