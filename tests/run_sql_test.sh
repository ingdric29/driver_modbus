#!/usr/bin/env bash
# Opt-in: requires the dedicated local MariaDB fixture described in README.
set -euo pipefail
cd "$(dirname "$0")/.."
test_dir=$(mktemp -d /tmp/driver-sql.XXXXXX)
trap 'rm -rf "$test_dir"' EXIT
read -r -a extra_cpp <<< "${CPPFLAGS:-}"
read -r -a extra_ld <<< "${LDFLAGS:-}"
"${CXX:-g++}" -std=c++11 -pthread -fsanitize=address,undefined -Isrc/include "${extra_cpp[@]}" \
  tests/test_sql_session.cpp "${extra_ld[@]}" -lmysqlcppconn -o "$test_dir/test"
timeout 20 "$test_dir/test"
DRIVER_SQL_TEST=1 bash tests/run_state_test.sh
python3 tests/test_full_startup.py "${DRIVER_BINARY:-$PWD/.build/automonitorv1}"
echo 'PASS SQL integration and replication'
