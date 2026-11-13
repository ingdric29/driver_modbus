#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."
test_dir=$(mktemp -d /tmp/driver-state.XXXXXX)
trap 'rm -rf "$test_dir"' EXIT
read -r -a extra_cpp <<< "${CPPFLAGS:-}"
read -r -a extra_ld <<< "${LDFLAGS:-}"
includes=(-Isrc/include -I/usr/include/modbus "${extra_cpp[@]}")
"${CXX:-g++}" -std=c++11 -pthread "${includes[@]}" -Dmain=driver_entry -c src/main.cpp -o "$test_dir/main.o"
"${CXX:-g++}" -std=c++11 -pthread "${includes[@]}" tests/test_driver_state.cpp "$test_dir/main.o" \
  src/loadconfig.cpp src/ini.c src/INIReader.cpp src/cls_equipos.cpp src/conversiones.cpp \
  src/cls_cmd.cpp src/cls_defalarmas.cpp src/enviarmail.cpp src/cls_email.cpp \
  src/cls_defeventos.cpp src/cls_sms.cpp src/cls_logger.cpp src/rs232.c \
  "${extra_ld[@]}" -lmodbus -lmysqlcppconn -lcurl -lboost_filesystem -lboost_system -o "$test_dir/test"
mkdir "$test_dir/run"
cd "$test_dir/run"
"$test_dir/test"
echo 'RS232 parser and concurrent sample checks passed'
