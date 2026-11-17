#!/usr/bin/env bash
# Archivo: package-runtime.sh
# Descripción: Prepara una distribución de ejecución sin fuentes ni configuración privada.
# Necesita: Directorio de binarios compilados y destino nuevo opcional.
# Retornos: 0 al completar; error si falla una copia o el destino ya existe.

set -euo pipefail
cd "$(dirname -- "${BASH_SOURCE[0]}")/.."
bin_dir="${1:-bin}"
output="${2:-}"
test -f "$bin_dir/lib/libsnap7.so"
if [[ -z "$output" ]]; then
    mkdir -p dist
    output="$(mktemp -d dist/automonitor-$(uname -m).XXXXXX)"
else
    # Never merge into an existing installation or overwrite private files.
    mkdir -- "$output"
fi
mkdir -- "$output/bin" "$output/html"
mkdir -p -- "$output/bin/lib" "$output/licenses/snap7" "$output/share"
install -m 755 "$bin_dir/lib/libsnap7.so" "$output/bin/lib/libsnap7.so"
install -m 644 third_party/snap7/gpl.txt third_party/snap7/lgpl-3.0.txt \
    third_party/snap7/UPSTREAM.md "$output/licenses/snap7/"
# Corresponding Snap7 sources and build instructions, without application sources.
tar -czf "$output/share/snap7-source.tar.gz" -C third_party snap7
for name in automonitorv1 automonitorctl automonitor-mailcheck; do
    install -m 755 "$bin_dir/$name" "$output/bin/$name"
done
# The Python monitor reads this file for its version identification.
install -m 644 "$bin_dir/version.h" "$output/bin/version.h"
install -m 644 html/plantilla_emails.html html/logo_automonitor.png "$output/html/"
cat > "$output/LEEME.txt" <<'EOF'
AUTOMONITOR - distribucion Linux

Este directorio no contiene fuentes del driver, objetos, tests ni historial Git.
Los visores Python y version.h son necesarios para las herramientas.
Usar en la arquitectura y sistema compatibles con la compilacion.
Requiere Python 3, libmodbus, MySQL Connector/C++, curl y Boost
filesystem/system. Esas bibliotecas del sistema no se incluyen.
Snap7 se incluye como biblioteca compartida reemplazable en bin/lib/libsnap7.so.
Avisos LGPL/GPL: licenses/snap7/. Fuentes y receta: share/snap7-source.tar.gz.
El ejecutable resuelve Snap7 relativo a bin mediante $ORIGIN/lib.
La inclusion de Snap7 no habilita aun adquisicion Siemens S7.

Copiar configdb.cfg autorizado junto a este archivo y protegerlo con chmod 600.
No se incluye configuracion privada ni se crea el esquema SQL.
Arrancar desde bin: cd bin && ./automonitorv1
En otra terminal: ./bin/automonitorctl
El driver creara log/ y .runtime/ en el directorio de distribucion.
No iniciar dos drivers contra los mismos equipos y base de datos.
EOF
printf '\n[ OK ] Distribucion creada: %s\n' "$output"
printf '       Copiar configdb.cfg autorizado antes de arrancar.\n'
