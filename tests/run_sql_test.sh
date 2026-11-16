#!/usr/bin/env bash
# Archivo: run_sql_test.sh
# Descripción: Comprueba integración SQL, replicación y arranque/correo con perfiles reservados.
# Necesita: Intérprete y dependencias del runner; consultar tests/README.md para el perfil SQL reservado.
# Retornos: 0 al completar las comprobaciones; un error o aserción hace fallar la prueba.

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
python3 tests/test_full_startup.py "${DRIVER_BINARY:-$PWD/bin/automonitorv1}"
python3 tests/test_full_email.py "${DRIVER_BINARY:-$PWD/bin/automonitorv1}"
echo 'PASS SQL integration, replication and full SMTP pipeline'
