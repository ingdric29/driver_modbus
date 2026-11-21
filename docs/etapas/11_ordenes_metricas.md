# Órdenes Modbus — etapa 1: medición

Fecha: 2026-11-21 (America/Caracas). Rama: `automonitor-driver-V1.3.0`.

## Alcance

Se instrumenta el recorrido existente SQL → lote → FC06 → confirmación SQL.
Se conservan el consumo anticipado de `tbejecutar`, la carga del catálogo,
el orden por ID, las conexiones compartidas y los tres intentos heredados.
No cambia el esquema de producción ni la configuración de muestreo/históricos.

Los recorridos local y externo usan un helper común, conservando sus entradas,
contadores y retornos. El ID de fila SQL se obtiene mediante un parámetro de
salida opcional de `leer_tbejecutar`; los llamadores existentes siguen siendo válidos.

## Diagnósticos

En `log/devlog.log`, relativo al padre del directorio de ejecución:

- `COMMAND`: correlación `run`/`execution` dentro del proceso, origen, fila de
  solicitud y comando; resultado Modbus separado de la confirmación SQL.
- `attempts`, `error`: número real de llamadas FC06 y último error de transporte.
- `link_wait_us`: adquisición del enlace, incluida la posible creación de contexto.
- `connect_us`: preparación/reutilización o conexión del enlace.
- `wire_us`: suma del tiempo dentro de las llamadas libmodbus de escritura;
  incluye la espera de respuesta. Las pausas entre intentos quedan en `execute_us`.
- `mutex_wait_us`, `poll_sql_us`, `catalog_sql_us`, `execute_us`, `ack_us`:
  separación de las esperas y operaciones principales.
- `batch_us`: tiempo desde antes del bloqueo global hasta terminar el lote;
  excluye la publicación posterior de los diagnósticos.
- `COMMAND_METRICS`: resumen cada 60 segundos y al terminar el trabajador;
  totales/fallos/máximo desde el inicio y p50/p95 de las últimas 256 mediciones
  de cada etapa/origen. Percentiles por rango más próximo, no interpolados.

Las duraciones usan `steady_clock`. El identificador `run` usa hora de pared
solo para correlación; no es un identificador persistente ni una garantía de
unicidad entre máquinas. El ID de `tbcmd` puede reutilizarse en varias ejecuciones.
No se interpreta el timestamp SQL existente como tiempo confiable de creación.

Las mediciones ocupan memoria acotada y no crecen con las ejecuciones.
Los diagnósticos nuevos se publican después de liberar el bloqueo del catálogo.
El lote se cita, limita y limpia de controles para conservar una línea por evento.
No se incluyen credenciales ni contenido de correo en los registros nuevos.

## Protección de alarmas

El catálogo completo y las acciones de alarmas permanecen disponibles. Esta
etapa no cambia el emisor SMTP ni sus destinatarios, plantillas o indicadores
de envío. El bloqueo compartido existente sigue pendiente de la etapa 6.

La nueva integración usa el binario real, MariaDB temporal con puerto libre,
un servidor Modbus TCP y SMTP TLS/AUTH locales. Ejecuta órdenes mientras se
activa una alarma `CMD[90];EMAIL[7];`, y verifica desactivación/reactivación,
fallo Modbus y fallo de confirmación SQL sin reenvío automático del comando.
Todos los destinatarios pertenecen a `example.invalid`.

## Validación y comandos

Instalación/pruebas en Raspberry, como usuario normal:

```bash
bash install.sh --test-command-sql
```

Con dependencias ya instaladas se admite `--skip-packages`. El nuevo flag instala
MariaDB servidor/cliente cuando corresponde y añade la prueba aislada; no utiliza
las credenciales de `configdb.cfg` ni el esquema de producción. Para un prefijo
no estándar admite `COMMAND_TEST_SYSROOT` o `S7_TEST_SYSROOT` (predeterminado `/usr`).

Consulta de diagnósticos desde `bin`:

```bash
rg 'COMMAND(_METRICS)? ' ../log/devlog.log
```

Resultado: `bash install.sh --skip-packages --test-command-sql` completó sus
nueve pasos en el entorno cloud, con dependencias previamente instaladas.
Compilación sin warnings, regresión ordinaria aprobada (unitarias ASan/UBSan,
estado/alarmas, SMTP, adquisición local S7/Modbus, monitor y distribución), y
la nueva integración SQL/FC06/SMTP aprobada. No se instalaron paquetes APT en
esta ejecución cloud ni se accedió a equipos físicos.

La referencia previa al cambio de planificación, con ambos períodos de órdenes
en 200 ms, dio 478,5 / 1006,7 / 959,9 ms desde el retorno del cliente SQL hasta
la recepción FC06; mediana 959,9 ms. Son tres observaciones locales con carga
de alarmas, no un benchmark estadístico ni una predicción para Raspberry.

La prueba detectó correctamente tres rechazos Modbus, una escritura aceptada
con fallo posterior de confirmación SQL y tres correos de alarma aceptados por
el SMTP TLS local, sin repetición de los ya aceptados. `ack=ok` significa que
el helper SQL terminó sin excepción; el control de filas afectadas permanece
pendiente. El consumo anticipado sigue siendo el comportamiento heredado.

## Limitaciones y etapas siguientes

Estas métricas observan fallos; todavía no añaden cola persistente, estados SQL,
validación de rangos, filtrado SQL por lote ni aislamiento de correos.
La aceptación Modbus no demuestra el efecto final del proceso físico.

Las siguientes etapas acordadas son: 2) sondeo monotónico; 3) selección del lote
separada del catálogo de alarmas; 4) validación; 5) seguimiento persistente;
6) aislamiento y concurrencia por enlace. La bandeja persistente de correos
requiere diseño específico y no se considera implementada por esta medición.
