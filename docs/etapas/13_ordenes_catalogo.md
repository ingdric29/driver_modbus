# Órdenes Modbus — etapa 3: lotes privados y catálogo de alarmas

Fecha: 2026-11-23 (America/Caracas). Rama: `automonitor-driver-V1.3.0`.
Base: `25a1371`. El usuario confirmó que las pruebas físicas de las etapas
anteriores funcionaron correctamente, incluidas las alarmas y el correo.

## Cambios

- Cada solicitud carga una lista privada del lote solicitado, filtrada por
  concentrador y `hab_ejecutar<>0`, ordenada por ID del comando.
- Se elimina el `COUNT` previo y la carga de comandos ajenos al lote. La consulta
  parametrizada mantiene una preselección compatible con el índice existente de
  `lote`, más una comparación exacta sensible a mayúsculas y espacios.
- Se conserva la equivalencia heredada entre lote NULL y vacío, y se normaliza
  la comparación a utf8mb4 para columnas Latin-1 como las del esquema real.
- El límite silencioso de 128 pasa a un límite explícito de 4096 filas por
  catálogo o lote. Se pide una fila adicional para detectar exceso y se rechaza
  toda la carga antes de escribir; nunca se ejecuta solo un prefijo del lote.
- El catálogo global `cmd` queda reservado a las acciones locales de alarmas.
  Los lotes externos no pueden reemplazar sus definiciones, aunque usen los
  mismos IDs con direcciones y valores distintos.
- El catálogo de alarmas se carga al iniciar cada generación y se actualiza al
  recargar el catálogo. Editar solamente `tbcmd` no dispara por sí mismo esa
  recarga: usar el mecanismo existente de recarga/reinicio cuando cambien sus
  acciones. Las órdenes normales sí leen su lote actualizado en cada solicitud.
- El trabajador de órdenes ya no mantiene `GLOBAL_command_mutex` durante SQL,
  comunicación ni registro. Las listas privadas no comparten memoria mutable
  con las alarmas. `mutex_wait_us=0` indica que este recorrido no toma ese bloqueo.
- Lecturas y escrituras conservan la exclusión por enlace de `ModbusLease`;
  no se permite acceso simultáneo al mismo contexto Modbus.
- Un fallo de carga local invalida las sesiones SQL de ese hilo y se registra;
  ya no deshabilita indebidamente el acceso externo como efecto de ese fallo local.

## Verificación

La prueba real SQL → FC06 → SQL se amplía con:

- Lote de 130 comandos: todos se escriben y confirman en orden.
- Acción `CMD[9000]` situada después de los primeros 128 elementos del catálogo.
- Comandos deshabilitados, otros lotes y diferencias de mayúsculas excluidos.
- Lotes con comillas, nombre Latin-1 y lotes NULL/vacío.
- Rechazo completo de un lote de 4097 filas, sin transmitir al PLC.
- IDs locales/externos iguales con direcciones distintas: la alarma conserva
  su definición local después de ejecutar la orden externa.
- Correo de activación/desactivación/reactivación, tres rechazos Modbus,
  fallo de confirmación SQL sin reenviar, adquisición y cierre ordenado.

Los destinos son MariaDB, PLC TCP y SMTP TLS temporales en loopback.
No se modificó el esquema ni la configuración de la Raspberry.

Verificación aprobada: el instalador completó los diez pasos con regresión
ordinaria, SQL mixto Siemens/Modbus y ambos perfiles de órdenes. Tras incorporar
los casos reales NULL/Latin-1 se recompiló sin warnings y se repitió
`make test-command-sql`: ambos perfiles aprobaron con seis correos TLS aceptados
en total. Las medianas orientativas de tres órdenes fueron 134,6 ms (externo
deshabilitado) y 148,4 ms (habilitado), con período local de 200 ms; no son una
garantía temporal para equipos físicos. La instalación usó `--skip-packages`.

Comando en instalaciones con las dependencias existentes:

```bash
bash install.sh --skip-packages --test-command-sql --test-s7-sql
```

## Alcance pendiente

La etapa 4 añadirá validación previa de todos los comandos del lote. Se conserva
por ahora el consumo anticipado de la solicitud y la política de tres intentos.
El usuario dejó expresamente pendiente la etapa 5 (seguimiento persistente).
El envío SMTP sigue síncrono dentro del trabajador de alarmas: no existe aún
cola de correo ni aislamiento completo del motor de alarmas ante SMTP lento.
Esta etapa evita que el SMTP retenga además al trabajador normal de órdenes
por el bloqueo global, pero no cambia el orden de las acciones de una alarma.
