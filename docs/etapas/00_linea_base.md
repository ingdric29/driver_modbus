# Etapa 0: línea base sin SQL

Fecha: 17 de noviembre de 2026.

## Objetivo y alcance

Preparar la rama `automonitor-modbus-driver-V1.2.0-pruebas` y verificar el comportamiento previo a incorporar Snap7. Base inspeccionada: `9a04c039ef75d47af480687baacb388f072768bb`. El checkout estaba limpio antes de activar la rama. No se modificó lógica del driver ni configuración privada.

## Entorno

Debian 13 x86_64; GCC/G++ 14.2.0; GNU Make 4.4.1; Python 3.12.14. Bibliotecas locales bajo `/workspace/driver-modbus-env/sysroot/usr`: libmodbus 3.1.11, MySQL Connector/C++ 1.1.12 y Boost 1.83; curl 8.14.1 con OpenSSL del sistema. clang-format 19.1.7 y MariaDB 11.8.6 disponibles; MariaDB no se inició para estas pruebas.

Se usaron dos trabajos de compilación en una máquina con cinco CPU disponibles. Objetos en `.build/` y ejecutables en `bin/`; las rutas están ignoradas por Git.

## Comando reproducible en este entorno

```bash
cd /workspace/driver_modbus
source /workspace/driver-modbus-env/activate.sh
make -j2 test \
  INCLUDES='-Isrc/include -I/workspace/driver-modbus-env/sysroot/usr/include -I/workspace/driver-modbus-env/sysroot/usr/include/modbus' \
  LIBS='-L/workspace/driver-modbus-env/sysroot/usr/lib/x86_64-linux-gnu -Wl,-rpath,/workspace/driver-modbus-env/sysroot/usr/lib/x86_64-linux-gnu'
```

Salida completa local: `/workspace/driver-modbus-env/stage0-test.log`. El comando finalizó con código 0.

## Resultados

- Compilación nativa del driver y generación de herramientas: aprobadas.
- Runner unitario con ASan/UBSan: decodificación Modbus, trabajadores, periodos, ventanas de muestras, lotes pendientes, expresiones de alarmas, reconexión, formato numérico/fechas, agrupación de registros, transportes Modbus y monitor: aprobado.
- Transporte Modbus: TCP local y RTU mediante pseudoterminales; no hardware industrial.
- Runner de estado enlazado al driver real: aprobado, incluyendo publicación concurrente y parser RS232.
- Correo con servidores locales simulados y comparación con fixture original: aprobado.
- Herramienta mailcheck: 3 pruebas aprobadas.
- Cliente del monitor: 4 pruebas aprobadas.
- Arranque sin licencia: 3 escenarios aprobados (claves ausentes, cero y mal formadas). La indisponibilidad SQL es esperada en esas comprobaciones; no acredita funcionamiento con base de datos.
- No se ejecutó `make test-sql`, no se conectó a PLC y no se ejecutó la adquisición productiva.

## Documentación y cierre

Se incorpora `docs/acuerdos_siemens_s7.md` con alcance, reglas, etapas e informe obligatorio por etapa. La instalación de herramientas anterior es preparación y no cambia la línea base funcional.

Los scripts antiguos de onboarding corresponden a main y no deben reutilizarse sin revisar. Para esta rama el comando anterior reemplaza la receta antigua que trataba los objetos ARM. No hay fallos pendientes en las pruebas requeridas de esta etapa.

Etapa 0 validada. El SHA del commit de cierre y la confirmación del push se comunican después de crear el commit, para no introducir autorreferencia en este informe. La prueba en Raspberry y S7-1200 permanece reservada a la etapa 6.
