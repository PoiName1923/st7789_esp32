#!/bin/sh
# Host tests for hardware-independent logic. Needs any C compiler: CC=gcc|clang|"zig cc"
set -e
cd "$(dirname "$0")/../.."
C=components
${CC:-cc} -std=c11 -Wall -Wextra -Werror -O1 \
  -I$C/display/include -I$C/sensor/include -I$C/weather/include \
  -I$C/connectivity/include -I$C/app_controller/include -I$C/animation/include \
  -I$C/settings/include -I$C/web/include \
  tests/host/test_main.c \
  $C/display/display_orient.c $C/display/board_pins.c $C/sensor/dht11_proto.c \
  $C/weather/weather_parse.c $C/app_controller/robot_fsm.c $C/animation/gif_assets.c $C/animation/animation_name.c \
  $C/settings/settings_validate.c $C/connectivity/connectivity_state.c $C/app_controller/robot_command.c $C/web/web_auth.c \
  -lm -o tests/host/test_runner
./tests/host/test_runner
