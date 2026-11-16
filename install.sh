#!/usr/bin/env bash
set -Eeuo pipefail

# Debian / Raspberry Pi OS. Ejecutar como usuario normal: bash install.sh
REPO_DIR="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
PASO="Inicio"
fallo() {
    local codigo=$?
    printf '\n[ ERROR ] Falló el paso: %s (código %s).\n' "$PASO" "$codigo" >&2
    exit "$codigo"
}
trap fallo ERR
paso() {
    PASO="$1"
    printf '\n[ PASO ] %s\n' "$PASO"
}
if (( EUID == 0 )); then
    printf '[ ERROR ] Ejecuta como usuario normal: bash install.sh\n' >&2
    printf '          Se solicitará sudo para instalar paquetes.\n' >&2
    exit 1
fi
for herramienta in apt-get sudo; do
    if ! command -v "$herramienta" >/dev/null 2>&1; then
        printf '[ ERROR ] Falta la herramienta: %s\n' "$herramienta" >&2
        exit 1
    fi
done
cd "$REPO_DIR"
if [[ ! -f makefile || ! -f src/main.cpp ]]; then
    printf '[ ERROR ] No se encontró el proyecto junto al instalador.\n' >&2
    exit 1
fi
printf '\n  AUTOMONITOR / INSTALACION LINUX\n'
printf '  Directorio: %s\n' "$REPO_DIR"
paso "Comprobar permisos para instalar paquetes"
sudo -v
paso "Actualizar el índice de paquetes"
sudo apt-get update
paso "Instalar o actualizar dependencias"
sudo apt-get install -y \
    build-essential git pkg-config \
    libmodbus-dev libmysqlcppconn-dev libcurl4-openssl-dev \
    libboost-filesystem-dev libboost-system-dev \
    python3 openssl ca-certificates
paso "Compilar el driver y sus herramientas"
make -j2
paso "Ejecutar pruebas locales"
make test
paso "Verificar versiones"
./bin/automonitorv1 --version
./bin/automonitorctl --version
paso "Preparar distribucion sin fuentes ni configuracion privada"
make dist
paso "Limpiar objetos intermedios"
make clean-objects
printf '\n[ OK ] Instalación y pruebas completadas.\n'
if [[ -f configdb.cfg ]]; then
    printf '\n  Se conservó configdb.cfg sin modificaciones.\n'
    printf '  Comprueba que corresponde a esta instalación.\n'
else
    printf '\n[ PENDIENTE ] Preparar configdb.cfg antes de arrancar.\n'
fi
printf '\n  Para iniciar desde este repositorio:\n'
printf '    cd "%s/bin"\n' "$REPO_DIR"
printf '    ./automonitorv1\n'
printf '\n  Para consultar el monitor desde otra terminal:\n'
printf '    "%s/bin/automonitorctl"\n' "$REPO_DIR"
printf '\n  Antes de arrancar, evita otra instancia sobre los mismos\n'
printf '  equipos y base de datos.\n'
