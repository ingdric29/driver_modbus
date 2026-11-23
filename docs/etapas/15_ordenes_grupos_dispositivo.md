# Ajuste de etapa 4 — grupos independientes de dispositivo

Fecha: 2026-11-23 (America/Caracas). Rama: `automonitor-driver-V1.3.0`.
Base: `b6f3cea`.

## Motivo y alcance

El lote real `Ajuste` contenía órdenes válidas de `PLC_TM172` y tres del
`PLC_TM172_INTERNO` deshabilitado. La validación global bloqueaba el setpoint
válido. El usuario aprobó agrupar por dispositivo y procesar grupos independientes.

La selección SQL conserva lote, concentrador y habilitación del comando.
Se planifica cada grupo por nombre de dispositivo dentro del origen SQL:

1. Los grupos deshabilitados se omiten por completo, incluso con campos NULL.
   No se abre su enlace ni se confirma su ejecución en SQL.
2. Se validan todos los comandos de cada grupo habilitado. Una configuración
   inválida rechaza solo ese dispositivo antes de enviar su primer comando.
3. Se ejecutan los grupos válidos conservando el orden global por ID. Una
   secuencia A/B/A continúa como A/B/A; no se reordena como A/A/B.
4. Un error de transporte no impide procesar comandos de otros grupos.

Varios dispositivos válidos de un lote se ejecutan todos; no se elige un PLC
arbitrariamente. El nombre de dispositivo identifica el grupo, no su IP:
varios pueden compartir pasarela. Se conserva la exclusión por enlace Modbus.
Si comparten un enlace caído, la espera de reconexión de ese enlace puede afectar
a ambos; la separación lógica no evita esa dependencia física. La prueba de
aislamiento de fallos usa dos servidores con puertos independientes.

## Diagnóstico

- `validation=skipped`: dispositivo deshabilitado, con nombre y motivo, WARN.
- `validation=rejected`: grupo inválido, con el comando que origina el rechazo.
- `status=accepted`: comandos procesados sin errores; puede haber `skipped>0`.
- `status=skipped`: todos deshabilitados, WARN.
- `status=partial`: hay grupos rechazados y otros comandos procesados, WARN.
  Los resultados individuales determinan cuáles aceptó Modbus y confirmó SQL.
- `status=rejected`: grupos inválidos sin escrituras procesadas.
- `status=failed`: fallo de carga/SQL/transporte sin rechazo parcial de configuración.

`skipped` y `rejected` cuentan las órdenes omitidas y rechazadas; `matched`
cuenta las procesadas por el ejecutor. La omisión por habilitación no se cuenta
como error de validación. Los fallos reales conservan ERROR y las métricas INFO.
El retorno es negativo si hubo errores, aunque otros comandos hayan funcionado.

## Verificación

La integración real SQL/Modbus/SMTP temporal añade: caso Ajuste con valor 52,
grupo inválido aislado, deshabilitado con NULL, orden intercalado entre equipos
y fallo de transporte con continuación del otro grupo. Comprueba que solo se
confirman las órdenes aceptadas. Se mantienen alarmas CMD, tres correos de
activación/desactivación/reactivación por perfil, límites, Latin-1, 130 comandos,
lectura mixta Siemens/Modbus y cierre. El monitor distingue WARN de ERROR.

Resultado aprobado: compilación sin warnings, regresión ordinaria y SQL mixto
Siemens/Modbus mediante el instalador con `--skip-packages`; integración final
`make test-command-sql` aprobada en ambos perfiles local/externo. Se comprobaron
seis correos TLS aceptados en total y confirmaciones SQL únicamente de órdenes
aceptadas. Las medianas orientativas de tres solicitudes fueron 155,2 ms y
147,3 ms con período local de 200 ms; no son garantías físicas. Verificación de
versiones y distribución aprobadas; `configdb.cfg` conservó su SHA256.

## Instalación y límites

```bash
bash install.sh --skip-packages --test-command-sql --test-s7-sql
```

No requiere cambios SQL ni activar el equipo interno. Se conserva el catálogo
de acciones de alarmas y el envío SMTP. Permanecen el límite global de carga de
4096, el consumo anticipado de solicitudes y los reintentos. Los grupos no son
transacciones atómicas del PLC. La etapa 5 sigue pendiente por decisión del
usuario y el aislamiento completo de SMTP de etapa 6 también continúa pendiente.
Las pruebas cloud no sustituyen la validación física en Raspberry.
