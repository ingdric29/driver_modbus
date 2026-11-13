#!/bin/bash
# Obtener la ruta del directorio en el que está el script
# Modificación hecha por: Ing. Javier Flores 12/11/24

DIR_SCRIPT="$(dirname "$(realpath "$0")")"

# Crear el directorio 'autostart' si no existe
mkdir -p "$DIR_SCRIPT/autostart"

function run() {
    FECHA=$(date +%Y-%m-%d)
    HORA=$(date +%H-%M-%S)
    echo "Activando el programa devlogplus v1.0"
    echo "-------------------------------------"
    
    # Detener procesos existentes (ignorar errores si no están corriendo)
    sudo killall devlogplus || true
    sudo killall automonitorv1 || true
    sudo killall orquesta || true
    
    # Cambiar al directorio donde está el script
    cd "$DIR_SCRIPT"
    
    # Validar si el archivo ejecutable existe y tiene permisos de ejecución
    if [ ! -x "$DIR_SCRIPT/bin/orquesta" ]; then
        echo "Archivo $DIR_SCRIPT/bin/orquesta no encontrado o no es ejecutable"
        exit 1
    fi

    # Iniciar el programa y registrar la salida
    sudo nohup "$DIR_SCRIPT/bin/orquesta" &> "$DIR_SCRIPT/autostart/nohup.out"
    env >> "$DIR_SCRIPT/autostart/variablesenv"
    date >> "$DIR_SCRIPT/autostart/fechaejecucion"
}

run
exit


