# Etapa 5: agrupación, monitor y operación local

Fecha: 17 de noviembre de 2026. Rama: automonitor-modbus-driver-V1.2.0-pruebas.

## Cambios

- `s7_batch.h` planifica solo tags vencidos, por PLC/DB, agrupando bytes contiguos y superpuestos sin rellenar huecos ni adelantar periodos. Limita por PDU negociada menos 18 bytes de respuesta de lectura (núcleo Snap7 fijado); inicialmente 222 bytes conservadores. El transporte mantiene además la fragmentación nativa.
- Un bloque entrega el mismo timestamp de finalización a todas sus muestras. Se rechazan resultados no finitos; el estado conserva precisión double.
- Si un bloque es rechazado por error remoto, se intenta cada tag para aislar direcciones inválidas. No se hace ese fallback ante fallo de transporte, que usa reconexión progresiva.
- El monitor añade dirección S7 completa, diagnóstico por tag y `s7_links` con PDU, intentos, conexiones, llamadas DBRead y errores por dispositivo/generación. Los contadores globales conservan la semántica de resultados por tag y acumulan intentos/reutilización de conexiones. DBRead no equivale a paquetes TCP.
- Diagnósticos idénticos no agregan un evento por ciclo; el buffer existente sigue acotado. El visor `--once --device` muestra dirección y error, y la vista simple resume las sesiones S7.
- SIGHUP valida un catálogo candidato antes de parar la generación vigente. Un candidato inválido deja la adquisición activa y genera aviso. Uno válido detiene/une los trabajadores, descarta sesiones/catálogos anteriores y arranca otra generación con el mismo PID/socket. SIGINT/SIGTERM conservan cierre interrumpible y esperan solo operaciones acotadas.
- La prueba de estado admite la ubicación de Snap7 según BIN_DIR. No hay cambios de esquema, escrituras a PLC, persistencia SQL ni servicios nuevos.

## Pruebas y resultados

`make -j2 test` con los overrides de etapa 0 finalizó con código 0. Log: `/workspace/driver-modbus-env/stage5-test.log`.

- Unitario de agrupación con ASan/UBSan: huecos, límites de DB, solapamientos, selección solo de tags vencidos, límite útil y partición de 1000 bytes: aprobado.
- Cuatro pruebas Python con el driver real, dos servidores Snap7 y uno Modbus: aprobadas.
- Los ocho tipos, precisión DWORD, escala, caída de un PLC sin bloquear los otros, recuperación y rechazo de configuración: aprobados.
- 600 tags contiguos: todos los bytes contrastados con el fixture, una sesión y como máximo 6 llamadas DBRead en la ventana observada, en lugar de 600 lecturas individuales. La PDU y los contadores se verifican por monitor.
- Periodos de 50 y 250 ms: el tag rápido acumula más del doble de lecturas; la agrupación no fuerza la frecuencia del lento.
- Un tag al final del DB sigue válido aunque un vecino fuera del DB haga fallar el bloque; el inválido no publica valor ni provoca una nueva conexión.
- Dos recargas válidas, cambio de escala y deshabilitación de un PLC: nueva generación, mismo PID y catálogo actualizado.
- Recarga inválida: generación y valor anteriores conservados, mientras continúan aumentando las lecturas.
- Segunda instancia sobre la misma instalación: rechazada sin detener la primera.
- SIGTERM con solicitudes locales activas: salida 0 en menos de 2 s y socket eliminado; visor informa indisponibilidad sin iniciar adquisición.
- Regresión previa (estado, Modbus TCP/RTU, RS232, monitor/TUI, correo y herramientas), integridad y ABI Snap7: aprobada. No se ejecutó make test-sql.
- Paquete trasladado: arranque/versionado, biblioteca propia, fuentes/licencias y validación de configuración local de ejemplo: aprobados (`stage5-package.log`).

La reconstrucción física ARM64 y las respuestas del S7-1200 real no están incluidas en estos resultados. El protocolo JSON conserva versión 1 con campos aditivos; clientes anteriores pueden ignorarlos.

## Historial de etapas anteriores

- Etapa 2: `075ea49a877fc31a1e4d8050b65b0173ad0f3041`.
- Etapa 3: `5b1a24d24596a56bf6a6fc101cdd93abfa582633`.
- Etapa 4: `7637867ac6d09caa6952c72ef82b4b4d02390b8b`.

Cada una tiene informe, commit local y SHA remoto comprobado antes de comenzar la siguiente. El SHA de este cierre se comunica después de crearlo.

## Límites y siguiente acción

La adquisición S7 está disponible en el modo local, junto con MBTCP. El arranque normal del driver, su catálogo SQL y los demás protocolos heredados se conservan; Siemens aún no se integra con SQL, aplazado por el usuario.

Etapa 5 validada sin fallos requeridos pendientes. La etapa 6 sigue pendiente de ejecución física; no se declara aprobada ni se crea su commit de cierre. El usuario confirmó equipos preparados y acceso por SSH, pero no proporcionó un destino SSH concreto al entorno cloud. No hay un alias SSH configurado para ese destino en esta máquina; no se ha verificado autenticación SSH. La guía `docs/validacion_raspberry_s7.md` permite ejecutar las comprobaciones desde la sesión SSH del usuario y reunir evidencia para el informe físico.
