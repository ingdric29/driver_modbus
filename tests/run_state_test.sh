#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."
test_dir=$(mktemp -d /tmp/driver-state.XXXXXX)
trap 'rm -rf "$test_dir"' EXIT
read -r -a extra_cpp <<< "${CPPFLAGS:-}"
read -r -a extra_ld <<< "${LDFLAGS:-}"
read -r -a extra_flags <<< "${TEST_CXXFLAGS:-}"
includes=(-Isrc/include -I/usr/include/modbus "${extra_cpp[@]}")
"${CXX:-g++}" -std=c++11 -pthread "${extra_flags[@]}" "${includes[@]}" -Dmain=driver_entry -c src/main.cpp -o "$test_dir/main.o"
"${CXX:-g++}" -std=c++11 -pthread "${extra_flags[@]}" "${includes[@]}" tests/test_driver_state.cpp "$test_dir/main.o" \
  src/driver_runtime.cpp src/command_worker.cpp src/maintenance_worker.cpp \
  src/serial_worker.cpp src/event_worker.cpp src/alarm_worker.cpp \
  src/catalog_lookup.cpp src/runtime_support.cpp \
  src/acquisition.cpp src/persistence.cpp src/config_file.cpp src/device_catalog.cpp src/action_catalog.cpp \
  src/command_repository.cpp src/sql_repository.cpp src/replication.cpp \
  src/retention.cpp src/catalog_lifecycle.cpp src/repository_support.cpp \
  src/ini.c src/INIReader.cpp src/cls_equipos.cpp src/conversiones.cpp \
  src/cls_cmd.cpp src/cls_defalarmas.cpp tests/fake_email.cpp src/cls_email.cpp \
  src/cls_defeventos.cpp src/cls_sms.cpp src/cls_logger.cpp src/rs232.c src/monitor_server.cpp \
  "${extra_ld[@]}" -lmodbus -lmysqlcppconn -lcurl -lboost_filesystem -lboost_system -o "$test_dir/test"
mkdir "$test_dir/run"
cd "$test_dir/run"
timeout 30 "$test_dir/test"
echo 'RS232 parser and concurrent sample checks passed'
