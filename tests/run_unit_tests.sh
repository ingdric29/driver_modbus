#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."
test_dir=$(mktemp -d /tmp/driver-unit.XXXXXX)
trap 'rm -rf "$test_dir"' EXIT
read -r -a extra_cpp <<< "${CPPFLAGS:-}"
read -r -a extra_ld <<< "${LDFLAGS:-}"
read -r -a extra_flags <<< "${TEST_CXXFLAGS:--fsanitize=address,undefined -fno-omit-frame-pointer}"
common=(-std=c++11 -pthread -Wall -Wextra -Isrc/include -I/usr/include/modbus "${extra_cpp[@]}" "${extra_flags[@]}")
for unit in modbus_decode workers periods sample_window pending_batch alarm_expression retry; do
  "${CXX:-g++}" "${common[@]}" "tests/test_${unit}.cpp" -o "$test_dir/$unit"
  timeout 15 "$test_dir/$unit"
  echo "PASS $unit"
done
"${CXX:-g++}" "${common[@]}" tests/test_numeric_format.cpp src/conversiones.cpp src/modbus_conversions.cpp src/datetime_conversions.cpp src/text_conversions.cpp src/network_utils.cpp -o "$test_dir/format"
timeout 15 "$test_dir/format"
echo 'PASS numeric_format'
for unit in register_batch modbus_transport; do
  "${CXX:-g++}" "${common[@]}" "tests/test_${unit}.cpp" "${extra_ld[@]}" -lmodbus -o "$test_dir/$unit"
  timeout 15 "$test_dir/$unit"
  echo "PASS $unit"
done

"${CXX:-g++}" "${common[@]}" tests/test_monitor.cpp src/monitor_server.cpp -o "$test_dir/monitor"
timeout 15 "$test_dir/monitor"
