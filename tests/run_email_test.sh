#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."
test_dir=$(mktemp -d /tmp/driver-email.XXXXXX)
trap 'rm -rf "$test_dir"' EXIT
read -r -a extra_cpp <<< "${CPPFLAGS:-}"
read -r -a extra_ld <<< "${LDFLAGS:-}"
read -r -a extra_flags <<< "${TEST_CXXFLAGS:--fsanitize=address,undefined -fno-omit-frame-pointer}"
"${CXX:-g++}" -std=c++11 -pthread -Isrc/include -I/usr/include/modbus "${extra_cpp[@]}" "${extra_flags[@]}" \
    tests/test_email_transport.cpp src/enviarmail.cpp src/conversiones.cpp src/cls_logger.cpp \
    "${extra_ld[@]}" -lcurl -lboost_filesystem -lboost_system -o "$test_dir/test-email"
python3 tests/test_email_transport.py "$test_dir/test-email"
