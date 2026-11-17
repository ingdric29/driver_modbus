# Etapa 4: adquisición integrada con configuración local

Fecha: 17 de noviembre de 2026. Rama: automonitor-modbus-driver-V1.2.0-pruebas.

## Implementación

El ejecutable existente incorpora `--check-local-config archivo.ini` y `--local-config archivo.ini`. Ese modo no carga configdb.cfg, no abre conexiones SQL y no persiste muestras. Se conserva el arranque normal con catálogo SQL, sin incorporar todavía Siemens a dicho catálogo.

`local_config.cpp` reutiliza INIReader; valida tipos, periodos, escalas, dispositivos/referencias, campos desconocidos y límites antes de conectar. `local_acquisition.cpp` reutiliza WorkerGroup, PeriodicDeadline, SampleWindow, ReadMeasurement, monitor y control de parada. Cada PLC S7 tiene trabajador y sesión propios. Modbus TCP usa `cls_equipos::leer()` y el transporte heredado en un único trabajador de protocolo, conservando la exclusión de sus buffers estáticos. El timeout MBTCP se aplica dentro de ese trabajador.

Las muestras S7 mantienen double; no se estrechan a los campos float de Modbus, evitando perder precisión en DWORD. Un fallo conserva el último valor válido identificado como antiguo/error, sin producir cero ni nueva muestra. Los periodos usan reloj monotónico y saltan intervalos vencidos. El monitor existente observa la misma instancia por socket Unix y su lock evita otra instancia sobre esa instalación.

El ejemplo sin credenciales `examples/local-acquisition.ini` se incluye también en la distribución. Guía detallada: `docs/siemens_s7.md`.

## Verificación

- `make test-local`: dos pruebas de integración con el binario real; configuración inválida/validación sin conexión y adquisición/recuperación/cierre.
- Fixture: dos servidores Snap7 independientes y un servidor libmodbus TCP de solo lectura, en loopback y puertos libres.
- Los ocho tipos se contrastan con valores conocidos; REAL=25 escalado a 50 y DWORD=4294967295 se publican correctamente.
- SQL: contadores de operaciones exitosas y fallidas permanecen en cero.
- Caída del primer S7: el segundo y Modbus continúan incrementando lecturas válidas. Recuperación del primero: ONLINE nuevamente.
- SIGTERM: salida 0, trabajadores unidos y socket eliminado.
- Configuraciones incompatibles, periodos cero, referencias inexistentes y campos desconocidos: rechazados antes de iniciar adquisición.
- `make -j2 test` con los overrides locales de etapa 0: código 0; log `/workspace/driver-modbus-env/stage4-test.log`. Regresión de estado, Modbus TCP/RTU, RS232, monitor, correo y herramientas aprobada.
- Paquete actualizado con ejemplo: `make test-snap7`, código 0; log stage4-package.log.

## Límites

Solo configuración local: los mapas SQL de Siemens y su persistencia permanecen aplazados. El modo local MBTCP admite word/word_signed y hr/rr; el modo normal mantiene todos sus protocolos e interpretaciones. Aún no hay agrupación S7 ni recarga local en caliente (etapa 5). No se ejecutaron pruebas SQL ni con equipos físicos.

Etapa 4 validada sin fallos requeridos pendientes. El SHA y resultado del push se informan después del commit.
