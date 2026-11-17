# Integración SQL Siemens y operación simultánea

Fecha: 17 de noviembre de 2026. Rama: automonitor-modbus-driver-V1.2.0-pruebas.

## Alcance y contrato

El usuario confirmó lectura física correcta variando valores en el S7-1200 y preparó la base en su Raspberry: dispositivo 52, modelo S7-1200_Rack, rack 0/slot 1 y cinco tags con direccion_siemens e interpretaciones s7_*. También confirmó respaldo del estado posterior al alta de tags y anterior al agregado de rack/slot. Esta entrega no declara realizada la prueba física de persistencia SQL; esa comprobación queda pendiente en la Raspberry.

El arranque habitual del driver ahora incorpora Siemens junto con Modbus TCP/RTU y RS232. No requiere un segundo proceso ni modificar qrytablacom: el nuevo catálogo une tbdispositivos con tbregistros por modelo y selecciona SIEMENS_S7 habilitados del concentrador configurado. No usa registro, funcion ni interpretaciones Modbus para S7. Si no existen dispositivos Siemens habilitados, no requiere las columnas nuevas ni tbregistros para esta ruta, conservando instalaciones anteriores.

Se reutilizan la adquisición agrupada, el transporte, WorkerGroup, monitor y sesiones SQL por hilo. Un trabajador por PLC entrega muestras double a un slot protegido; un escritor S7 independiente copia esas muestras sin bloquear adquisición durante I/O SQL. Los trabajadores existentes Modbus siguen activos. No se altera su persistencia ni se habilitan escrituras a PLC.

## Escritura

- tbinstantaneos: actualizar por tag/concentrador/nombredispositivo o insertar si no existe, preservando el contrato existente. dispositivo contiene el ID real de tbdispositivos.
- tbvalores: INSERT cuando registrable=1, respetando max(periodo, periodo_minimo_escritura_tbvalores). solocambios=1 registra la primera muestra y cambios respecto al último histórico confirmado; enteros/BOOL exactos y REAL con diferencia absoluta mayor a 0.0001 sobre valor escalado.
- registrable=0 no impide instantáneos.
- fechatiempo corresponde a adquisición; timedate usa CURRENT_TIMESTAMP de MariaDB.
- valorfloat usa double; valorstr usa representación compacta de 12 cifras significativas. No se reduce S7 a los float heredados de Modbus.
- Solo se envían muestras válidas nuevas. No se refecha una muestra antigua ni se fabrica cero ante caída de PLC.
- Instantáneo e histórico debidos de una muestra se confirman en la misma transacción preparada. Fallo previo al COMMIT: rollback/reintento a 1 s, conservando esa muestra pendiente. Resultado incierto durante COMMIT/restauración de autocommit: se registra y no se reintenta el lote para evitar duplicados automáticos; no se promete exactamente-una-vez.

Rastro estable en ambas tablas:

```text
Siemens_S7_1200;192.168.88.15:102;test_real;protocolo=SIEMENS_S7;rack=0;slot=1;dir=DB100.DBD8;tipo=s7_real;esc=1
```

Se validan longitudes de identidad/rastro, direcciones, tipos, periodos, escalas y límites del catálogo. No se aplican DDL ni se alteran datos de configuración desde el driver.

## Pruebas

- make test: regresión ordinaria completa sin SQL y pruebas locales S7/Modbus, con los overrides del entorno documentados en etapa 0.
- Nuevo target opt-in make test-s7-sql: inicia MariaDB temporal en un puerto loopback libre y crea solo una base fixture. No lee credenciales del usuario ni toca dbdevlog. Para paquetes extraídos: S7_TEST_SYSROOT=/workspace/driver-modbus-env/sysroot/usr; instalación de sistema: /usr por defecto.
- El test arranca el ejecutable real con su configuración SQL y dos protocolos, usando servidores Snap7/libmodbus locales. Comprueba IDs, rastro, DWORD=4294967295, actualización sin duplicar instantáneos, registrable=0, solo cambios y valores modificados en memoria del fixture.
- Un trigger temporal fuerza un fallo de histórico: el instantáneo de esa transacción se revierte y el reintento persiste después de retirar el trigger. Otros tags continúan adquiriendo.
- Caída de S7: no aparecen históricos nuevos de ese PLC, mientras Modbus sigue ONLINE; cierre normal del proceso.

Logs locales en /workspace/driver-modbus-env: s7-sql-regression-final.log y s7-sql-integration.log. La prueba de COMMIT incierto no inyecta una caída exacta en la ventana del COMMIT; se documenta el tratamiento, sin afirmar que ese escenario haya sido reproducido.

## Límites y operación

El buffer es RAM acotada: último valor por tag más una muestra pendiente de escritor; no es una cola histórica completa ni persistente. Muestras intermedias pueden omitirse durante SQL lento/fallido. En recarga/cierre los pendientes no confirmados se contabilizan en un aviso; no se trasladan a otra generación ni sobreviven a reinicio. No se implementan picos S7, acciones/alarmas S7 ni replicación adicional en esta entrega.

La carga se renueva con las generaciones del catálogo habitual. El modo --local-config conserva funcionamiento sin SQL. En Raspberry se debe actualizar/compilar, usar configdb.cfg autorizado existente y arrancar normalmente desde bin (sin --local-config). Verificar coincidencia exacta del concentrador, lecturas de ambos protocolos en monitor y filas de dispositivo 52 en ambas tablas. No se ejecutó contra equipos ni base productiva desde cloud.
