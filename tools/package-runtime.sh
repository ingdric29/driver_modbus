#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname -- "${BASH_SOURCE[0]}")/.."
bin_dir="${1:-bin}"
output="${2:-}"
if [[ -z "$output" ]]; then
    mkdir -p dist
    output="$(mktemp -d dist/automonitor-$(uname -m).XXXXXX)"
else
    # Never merge into an existing installation or overwrite private files.
    mkdir -- "$output"
fi
mkdir -- "$output/bin" "$output/html"
for name in automonitorv1 automonitorctl automonitor-mailcheck; do
    install -m 755 "$bin_dir/$name" "$output/bin/$name"
done
# The Python monitor reads this file for its version identification.
install -m 644 "$bin_dir/version.h" "$output/bin/version.h"
install -m 644 html/plantilla_emails.html html/logo_automonitor.png "$output/html/"
cat > "$output/LEEME.txt" <<'EOF'
AUTOMONITOR - distribucion Linux

Este directorio no contiene fuentes C++, objetos, tests ni historial Git.
Los visores Python y version.h son necesarios para las herramientas.
Usar en la arquitectura y sistema compatibles con la compilacion.
Requiere Python 3, libmodbus, MySQL Connector/C++, curl y Boost
filesystem/system. Las bibliotecas compartidas no se incluyen.

Copiar configdb.cfg autorizado junto a este archivo y protegerlo con chmod 600.
No se incluye configuracion privada ni se crea el esquema SQL.
Arrancar desde bin: cd bin && ./automonitorv1
En otra terminal: ./bin/automonitorctl
El driver creara log/ y .runtime/ en el directorio de distribucion.
No iniciar dos drivers contra los mismos equipos y base de datos.
EOF
printf '\n[ OK ] Distribucion creada: %s\n' "$output"
printf '       Copiar configdb.cfg autorizado antes de arrancar.\n'
