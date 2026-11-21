# Órdenes Modbus — etapa 2: planificación del sondeo

Fecha: 2026-11-21 (America/Caracas). Rama: `automonitor-driver-V1.3.0`.
Base: etapa 1, commit `9b16dda`.

## Problema corregido

El trabajador comparaba segundos de pared con `diferencia > período` y después
esperaba el menor período local/externo. Podía omitir un vencimiento exacto y
esperar otro ciclo. Los períodos fraccionarios quedaban condicionados a la
resolución de `time(NULL)` y el período externo influía incluso deshabilitado.

`CommandPollSchedule` reutiliza `PeriodicDeadline` con `steady_clock` y dos
plazos independientes. La primera consulta es inmediata; cada finalización
avanza al próximo vencimiento futuro de su origen, conservando la fase y
saltando los intervalos perdidos. Una operación larga no genera una ráfaga
de consultas para recuperar los vencimientos anteriores.

El trabajador espera hasta el siguiente plazo activo o el resumen de métricas,
con la espera interrumpible existente. Un origen externo deshabilitado por
configuración no se consulta ni modifica el próximo despertar. La inhibición
temporal de acceso externo conserva la política existente del repositorio SQL.
Tras una recarga se crea un planificador para la nueva generación.

Los timestamps globales heredados se siguen actualizando para diagnóstico,
pero no gobiernan el sondeo. La métrica `poll_lag` mide el retraso desde el plazo
previsto hasta iniciar el recorrido; la espera posterior del bloqueo se mide
separadamente como `mutex`.

## Compatibilidad y límites

- Se utilizan los valores existentes de `periodo_muestreo_tbejecutar` y
  `periodoext_muestreo_tbejecutar`. No se modifica `configdb.cfg` ni se impone
  un período nuevo a una instalación existente.
- Los períodos de lectura, `tbinstantaneos` y `tbvalores` permanecen iguales.
- El catálogo, la selección del lote, FC06 y sus reintentos son los de la etapa 1.
- Local y externo siguen ejecutándose secuencialmente en el mismo trabajador;
  un SQL lento, el bloqueo de una alarma o una escritura lenta todavía pueden
  retrasar otro origen. Esta etapa corrige planificación, no ese aislamiento.
- Las conexiones y el bloqueo del catálogo de alarmas se conservan. El emisor
  SMTP, plantillas, destinatarios y reglas de activación/desactivación no cambian.
- El consumo anticipado de solicitudes y el límite de 128 comandos siguen
  pendientes de las etapas posteriores; esta corrección no añade una cola SQL.

## Validación

`test_command_poll.cpp`, con tiempo simulado y ASan/UBSan, verifica:

- Tres sondeos locales a 0/5/10 s y seis externos a 0/2/4/6/8/10 s.
- Períodos 200/370 ms y consultas de 7 ms sin añadir deriva a cada período.
- Origen externo deshabilitado, reactivación y omisión de intervalos perdidos.
- Un retraso hasta 955 ms reprograma el siguiente sondeo de 200 ms a 1000 ms.

La integración del binario real ejecuta dos perfiles aislados: externo
deshabilitado y externo habilitado con otro esquema en la MariaDB temporal.
En ambos verifica FC06, alarmas CMD/EMAIL y continuidad del correo ante fallos Modbus/SQL;
en el segundo también ejecuta una orden originada en la base externa.

Comando recomendado en Raspberry:

```bash
bash install.sh --test-command-sql --test-s7-sql
```

El instalador conserva la configuración y ejecuta las pruebas sobre servidores
temporales; no inicia el driver sobre los equipos de producción.

Resultado: `bash install.sh --skip-packages --test-command-sql --test-s7-sql`
completó los diez pasos en cloud con dependencias previamente instaladas.
Compilación sin warnings; regresión ordinaria, planificación con ASan/UBSan,
integración SQL mixta Siemens/Modbus y ambos perfiles de órdenes aprobados.
Se verificaron seis correos SMTP TLS reales hacia el servidor local de pruebas
(tres por perfil) y acciones CMD de alarma conservadas. La suite ordinaria
también verificó los casos de rechazo SMTP y reintento de notificaciones.
No se enviaron correos a servicios externos ni se accedió a PLC físicos.

Comparación observada con sondeo local configurado a 200 ms:

| Perfil | SQL → recepción FC06, tres muestras (ms) | Mediana (ms) |
| --- | --- | --- |
| Planificación anterior, etapa 1 | 478,5 / 1006,7 / 959,9 | 959,9 |
| Nueva, externo deshabilitado | 74,4 / 152,2 / 147,1 | 147,1 |
| Nueva, externo habilitado a 370 ms | 56,3 / 152,3 / 137,0 | 137,0 |

El inicio de cada medida es el retorno del cliente SQL tras confirmar la
solicitud, no el instante exacto del COMMIT en el servidor. Tres muestras no
permiten establecer percentiles de producción ni comparar estadísticamente
los dos perfiles nuevos. No se interpreta la menor mediana del perfil externo
como ventaja causal. La prueba determinista fija los vencimientos esperados.

El SHA256 de `configdb.cfg` fue idéntico al terminar las dos etapas. La
validación física en Raspberry, sus tiempos de respuesta y su SMTP real
siguen pendientes. Las pruebas cloud se limitaron a servidores temporales.

## Seguimiento

La primera medición debe repetirse en Raspberry antes de elegir un período más
corto para producción. Un intervalo configurado no es un límite garantizado
de respuesta total: SQL, el enlace y las acciones concurrentes añaden tiempo.
La comparación cloud no demuestra tiempos físicos del PLC.

El siguiente bloque es la etapa 3: separar el catálogo completo de acciones
de alarma de la lista de ejecución y seleccionar el lote directamente en SQL.
Después siguen validación, seguimiento persistente y reducción de bloqueos.
