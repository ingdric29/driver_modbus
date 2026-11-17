# Etapa 6: validación pendiente en Raspberry Pi 5 y S7-1200

Esta guía se ejecuta dentro de la sesión SSH de la Raspberry. El usuario tiene
los equipos físicos preparados. El entorno cloud no dispone actualmente de un
host/usuario/alias SSH configurado para acceder a esa máquina, por lo que las
etapas locales no acreditan la prueba física. No enviar contraseñas en el chat.

## 1. Actualizar y compilar

No iniciar otra instancia sobre los mismos equipos. Detener la instancia de
adquisición de esa instalación por su mecanismo habitual. Conservar cualquier
cambio local; si Git informa conflictos o divergencias, detenerse y revisarlos,
sin reset, limpieza de archivos ni sobrescritura de configdb.cfg.

Desde el checkout existente (ajustar su ruta):

```bash
cd ~/driver_modbus
git status --short
git fetch origin automonitor-modbus-driver-V1.2.0-pruebas
git switch automonitor-modbus-driver-V1.2.0-pruebas
git merge --ff-only origin/automonitor-modbus-driver-V1.2.0-pruebas
git rev-parse HEAD
uname -a
g++ --version
make -j2
make test
./bin/automonitorv1 --version
file bin/automonitorv1 bin/lib/libsnap7.so
ldd bin/automonitorv1
```

Las dependencias Debian están documentadas en desarrollo.md. Snap7 se compila
desde los fuentes incorporados; no usar la biblioteca x86_64 del cloud ni el
binario adjunto como sustituto de esta compilación ARM64. No ejecutar
`make test-sql` ni iniciar MariaDB para este ensayo. Si `make test` falla,
guardar el error completo antes de continuar: no declarar pasada la etapa.

## 2. Configuración explícita de lectura

Crear un INI fuera del repositorio, por ejemplo `~/s7-validacion.ini`, usando
`examples/local-acquisition.ini` como referencia. No modificar configdb.cfg.
Confirmar la IP real del PLC (el antecedente es 192.168.88.15), puerto 102,
rack 0/slot 1, PUT/GET habilitado y DB100 sin acceso optimizado. No cambiar
configuración del PLC si el ensayo previo sigue funcionando.

Mapa del ensayo adjunto de lectura (contrastar con el proyecto TIA actual):

| Tag | Dirección | Tipo |
|---|---|---|
| test_bool | DB100.DBX0.0 | BOOL |
| test_int | DB100.DBW2 | INT |
| test_dint | DB100.DBD4 | DINT |
| test_real | DB100.DBD8 | REAL |
| test_counter | DB100.DBD12 | DINT |

Todos los tags requieren `period=1` y `scale=1` inicialmente. Añadir los tipos
BYTE/SINT/WORD/DWORD donde el mapa real lo permita, sin inventar variables ni
escribir al PLC para fabricar valores. El valor 4321 de DBW2 es un antecedente,
no una expectativa actual confirmada. Deshabilitar/eliminar el dispositivo
Modbus del ejemplo si no corresponde a un equipo del laboratorio autorizado.

```ini
[device:plc]
protocol=SIEMENS_S7
host=192.168.88.15
port=102
rack=0
slot=1
timeout_ms=1000

[tag:plc:test_real]
address=DB100.DBD8
type=REAL
period=1
scale=1
```

Ese fragmento solo incluye REAL; añadir las demás secciones del mapa que se
vayan a verificar antes de ejecutar la prueba completa.

## 3. Adquirir y guardar evidencia

```bash
cd ~/driver_modbus/bin
./automonitorv1 --check-local-config "$HOME/s7-validacion.ini"
./automonitorv1 --local-config "$HOME/s7-validacion.ini"
```

En otra sesión SSH, desde la raíz del repositorio:

```bash
./bin/automonitorctl --once --device plc
./bin/automonitorctl --json > "$HOME/s7-snapshot-inicial.json"
# Tras al menos 10 minutos, sin detener el proceso:
./bin/automonitorctl --json > "$HOME/s7-snapshot-final.json"
```

Contrastar valores con TIA o el ensayo independiente de lectura ya probado.
No ejecutar s7_write_test: la integración aprobada es de solo lectura. Guardar
versiones/HEAD, salida de compilación/pruebas, valores esperados/observados,
snapshots y duración real. Revisar que SQL permanece SIN_DATOS con cero
operaciones; que la PDU, latencias y conteos son coherentes; y que no hay errores
sin explicar. No incluir credenciales o configdb.cfg en el informe.

## 4. Recuperación, recarga y cierre

Si es operativamente seguro en este laboratorio, interrumpir solo el enlace del
PLC de prueba: observar ERROR/REINTENTANDO sin nuevos valores válidos, restablecer
el enlace y comprobar ONLINE. Si hay otros equipos en el ensayo, comprobar que
continúan adquiriendo. No detener ni reprogramar un PLC que controle un proceso
activo para realizar esta prueba.

Cambiar un periodo en una copia temporal del INI y reemplazar el original de
forma atómica. Enviar `kill -HUP PID` usando el PID del monitor: verificar nueva
generación con el mismo PID. Probar un candidato inválido: debe mantener la
generación previa y registrar el rechazo; restaurar después el INI válido.

Detener con Ctrl+C o SIGTERM; comprobar salida normal y desaparición del socket.
El visor debe indicar indisponible cuando el driver ya no está activo.

## Cierre de etapa

Compartir las salidas/resultados anteriores permite redactar
`docs/etapas/06_validacion_fisica.md`. Solo cuando las comprobaciones físicas
pasen se hará el commit y push de cierre de esa etapa. Una compilación cloud,
un servidor Snap7 simulado o la prueba independiente antigua no sustituyen esa
evidencia. Si aparece un fallo, diagnosticarlo y corregirlo antes del cierre.
