#!/usr/bin/env bash
# Archivo: run_unit_tests.sh
# Descripción: Comprueba ejecución de pruebas unitarias con sanitizadores y recursos locales.
# Necesita: Intérprete y dependencias del runner; consultar tests/README.md para el perfil SQL reservado.
# Retornos: 0 al completar las comprobaciones; un error o aserción hace fallar la prueba.

set -euo pipefail
cd "$(dirname "$0")/.."
test_dir=$(mktemp -d /tmp/driver-unit.XXXXXX)
trap 'rm -rf "$test_dir"' EXIT
read -r -a extra_cpp <<< "${CPPFLAGS:-}"
read -r -a extra_ld <<< "${LDFLAGS:-}"
read -r -a extra_flags <<< "${TEST_CXXFLAGS:--fsanitize=address,undefined -fno-omit-frame-pointer}"
common=(-std=c++11 -pthread -Wall -Wextra -Isrc/include -I/usr/include/modbus "${extra_cpp[@]}" "${extra_flags[@]}")
for unit in modbus_decode workers periods sample_window pending_batch alarm_expression retry s7_decode s7_batch command_metrics command_poll; do
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
