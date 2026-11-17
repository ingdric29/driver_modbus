#!/usr/bin/env bash
# Archivo: install.sh
# Descripción: Instala dependencias, compila, prueba y prepara la distribución Linux.
# Necesita: Usuario normal, sudo, APT, red y checkout del proyecto.
# Retornos: 0 al completar; conserva el código del paso fallido. No inicia servicios ni modifica el CFG.

set -Eeuo pipefail

# Debian / Raspberry Pi OS. Ejecutar como usuario normal: bash install.sh
REPO_DIR="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
PASO="Inicio"
OMITIR_PAQUETES=0
PROBAR_SQL=0
for argumento in "$@"; do
    case "$argumento" in
        --skip-packages) OMITIR_PAQUETES=1 ;;
        --test-s7-sql) PROBAR_SQL=1 ;;
        *) printf 'Uso: bash install.sh [--skip-packages] [--test-s7-sql]\n' >&2; exit 2 ;;
    esac
done
ETAPAS_TOTALES=$((8 + PROBAR_SQL))
ETAPAS_COMPLETADAS=0

# Dibuja el avance por etapas completadas, no por tiempo estimado.
# Parámetros: ninguno. Retorno: estado de printf; no usa controles de cursor.
mostrar_progreso() {
    local ancho=24 llenos porcentaje barra pendiente
    llenos=$((ETAPAS_COMPLETADAS * ancho / ETAPAS_TOTALES))
    porcentaje=$((ETAPAS_COMPLETADAS * 100 / ETAPAS_TOTALES))
    printf -v barra '%*s' "$llenos" ''
    printf -v pendiente '%*s' "$((ancho - llenos))" ''
    printf '[%s%s] %3d%% (%d/%d etapas completadas)\n' \
        "${barra// /#}" "${pendiente// /-}" "$porcentaje" \
        "$ETAPAS_COMPLETADAS" "$ETAPAS_TOTALES"
}

# Registra una etapa solo después de que sus comandos terminaron correctamente.
# Parámetros: ninguno. Retorno: estado de la impresión de progreso.
completar_paso() {
    ETAPAS_COMPLETADAS=$((ETAPAS_COMPLETADAS + 1))
    mostrar_progreso
}

# Informa el paso fallido y conserva el código de salida del comando original.
fallo() {
    local codigo=$?
    printf '\n[ ERROR ] Falló el paso: %s (código %s).\n' "$PASO" "$codigo" >&2
    exit "$codigo"
}
trap fallo ERR
# Anuncia la próxima etapa sin aumentar todavía el progreso completado.
# Parámetros: descripción del paso. Retorno: estado de printf.
paso() {
    PASO="$1"
    printf '\n[ PASO %d/%d ] %s\n' \
        "$((ETAPAS_COMPLETADAS + 1))" "$ETAPAS_TOTALES" "$PASO"
}
if (( EUID == 0 )); then
    printf '[ ERROR ] Ejecuta como usuario normal: bash install.sh\n' >&2
    printf '          Se solicitará sudo para instalar paquetes.\n' >&2
    exit 1
fi
if (( ! OMITIR_PAQUETES )); then
for herramienta in apt-get sudo; do
    if ! command -v "$herramienta" >/dev/null 2>&1; then
        printf '[ ERROR ] Falta la herramienta: %s\n' "$herramienta" >&2
        exit 1
    fi
done
fi
cd "$REPO_DIR"
if [[ ! -f makefile || ! -f src/main.cpp ]]; then
    printf '[ ERROR ] No se encontró el proyecto junto al instalador.\n' >&2
    exit 1
fi
printf '\n  AUTOMONITOR / INSTALACION LINUX\n'
printf '  Directorio: %s\n' "$REPO_DIR"
printf '  Progreso por etapas; su duración puede variar.\n\n'
mostrar_progreso
paso "Comprobar permisos para instalar paquetes"
if (( ! OMITIR_PAQUETES )); then sudo -v; fi
completar_paso
paso "Actualizar el índice de paquetes"
if (( ! OMITIR_PAQUETES )); then sudo apt-get update; fi
completar_paso
paso "Instalar o actualizar dependencias"
if (( ! OMITIR_PAQUETES )); then
paquetes_sql=()
if (( PROBAR_SQL )); then paquetes_sql=(mariadb-server mariadb-client); fi
sudo apt-get install -y \
    build-essential git pkg-config \
    libmodbus-dev libmysqlcppconn-dev libcurl4-openssl-dev \
    libboost-filesystem-dev libboost-system-dev \
    python3 openssl ca-certificates "${paquetes_sql[@]}"
fi
completar_paso
paso "Compilar el driver y sus herramientas"
make -j2
completar_paso
paso "Ejecutar pruebas locales"
make test
completar_paso
if (( PROBAR_SQL )); then
    paso "Probar SQL Siemens/Modbus en base temporal aislada"
    make test-s7-sql
    completar_paso
fi
paso "Verificar versiones"
./bin/automonitorv1 --version
./bin/automonitorctl --version
completar_paso
paso "Preparar distribucion con Snap7 y sus licencias, sin fuentes del driver ni configuracion privada"
make dist
completar_paso
paso "Limpiar objetos intermedios"
make clean-objects
completar_paso
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
