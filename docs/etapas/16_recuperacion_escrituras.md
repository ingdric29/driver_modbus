# Recuperación de escrituras y comparación V1.3.0

Fecha: 2026-11-24 (America/Caracas).
Rama: `automonitor-driver-V1.3.0`. Comparación acordada: `25a1371` y `2bbe930`.

## Evidencia y comparación

En Automonitor-A028, `Ajuste` carga 36 comandos y no rechaza ninguno por
configuración. Los IDs 1..17 seleccionados obtienen aceptación; el 26
(`iCfgDHRk`, registro 16404, valor 20) agota tres intentos con timeout 110.
Los siguientes IDs, incluidos 56..58, aparecen con cero intentos y EAGAIN (11).

Entre los dos commits:

- Se mantiene el orden por ID. No hay filtro por ID menor/mayor que 56 ni por fecha.
- `25a1371` carga el catálogo (máximo silencioso 128) y selecciona el lote en
  memoria. `2bbe930` selecciona el lote en SQL, usa listas privadas, valida por
  dispositivo y omite deshabilitados, con límite explícito de 4096.
- En este lote de 36 entradas válidas no se alcanza ninguno de esos límites.
- El transporte compartido y RetryBackoff son idénticos. El timeout configurable,
  FC06 y sus tres intentos se conservan para los datos válidos de esta captura.
- El bloqueo global de comandos se retiró del recorrido normal después de etapa 2;
  las acciones de alarmas pueden intercalarse entre operaciones, manteniendo la
  exclusión por enlace. No tenemos una comparación física controlada que permita
  atribuir a ese cambio una mayor frecuencia de timeouts.

Se compiló una copia aislada exportada de `25a1371`, sin cambiar ramas, y se
ejecutó la misma prueba contra ella y contra el binario `2bbe930`. En ambos casos
se reprodujo: ID26 recibe tres peticiones sin respuesta, IDs56/57 reciben cero.
El defecto de continuación ya estaba presente al terminar las etapas 1 y 2.
Esto no identifica el motivo del primer timeout del registro 16404 en el PLC real.

## Relación con las fechas de 2024

`cmd_datetime` se carga como dato, pero no gobierna selección, validación ni envío.
El driver solo intenta actualizarla después de aceptación Modbus. En los logs,
`ack=skipped` significa que no se llamó al helper SQL de confirmación.

El timeout del 26 activa la espera de reconexión; los comandos siguientes son
descartados demasiado pronto y no se confirman. Así conservan sus fechas previas.
Esto explica la correlación de esa ejecución, pero no prueba que llevaran fallando
desde 2024 ni que el PLC no se haya modificado por otras vías. La fecha también
puede quedar antigua si el PLC acepta y luego falla la confirmación SQL.
No se actualizan fechas artificialmente ni se usa su antigüedad para reenviar.

## Corrección

El trabajador SQL permite recuperar un comando únicamente si el ejecutor devuelve
EAGAIN antes de cualquier llamada FC06. Espera en pasos interrumpibles de hasta
20 ms y vuelve a comprobar ese mismo comando aún no transmitido.

El presupuesto compartido es de 500 ms de espera por lote, no 500 ms por comando.
La espera ocurre después de liberar el lease, permitiendo que lecturas y alarmas
usen o recuperen el enlace. No modifica RetryBackoff ni desactiva su protección.
No es un límite absoluto de ejecución: SQL, espera del mutex, conexión, respuesta
del PLC y planificación del sistema pueden añadir tiempo.

La recuperación adicional no reenvía comandos ya intentados o aceptados. Se
conservan los tres intentos nativos anteriores; no se implementa la política de
no repetición de acciones propuesta para la etapa 5, que continúa aplazada.

La espera interna EAGAIN no genera un ERROR de consola por cada comprobación.
Si finalmente no se logra enviar, el resultado sigue registrado como fallo real
con cero intentos. Otros errores de conexión conservan su diagnóstico.

## Diagnóstico ampliado

Cada orden incluye dispositivo, registro, valor bruto, texto del error y espera
de recuperación. `delivery` distingue:

- `accepted`: aceptación Modbus; consultar `ack` para el resultado SQL.
- `not_sent`: ninguna llamada FC06.
- `exception`: respuesta de excepción Modbus, sin inferir un límite concreto del PLC.
- `unknown`: hubo intento de escritura sin aceptación/ excepción concluyente.

El resumen mantiene `matched` (comandos procesados) y añade `sent` (comandos con
al menos una llamada FC06), `accepted` (aceptados por Modbus), `not_sent` y espera
de recuperación consumida. Una llamada FC06 no garantiza entrega de bytes ni
efecto físico: por eso se conserva la categoría indeterminada `unknown`.

## Verificación

La nueva prueba `--recovery-probe` se incorpora a `make test-command-sql`:

1. Los tres comandos parten con fechas de 2024. El 26 pierde sus respuestas;
   56 y 57 se envían una vez y actualizan sus fechas, sin reenviar el 26.
2. Una excepción Modbus conserva el fallo y permite continuar en el mismo enlace.
3. Veinticuatro comandos de un enlace indisponible comparten el presupuesto;
   el dispositivo independiente posterior funciona, sin falsificar fechas.
4. SIGTERM interrumpe la recuperación sin ejecutar los comandos posteriores.

Resultado: compilación sin warnings, regresión ordinaria, SQL mixto y los dos
perfiles SQL/SMTP aprobados mediante el instalador. La integración SQL se repitió
tras ajustar el diagnóstico EAGAIN, y la prueba final de cierre también aprobó.
La regresión del mismo enlace falla antes de la corrección y pasa después.
Todos los destinos son temporales/loopback; no se accedió a PLC físicos.

```bash
bash install.sh --skip-packages --test-command-sql --test-s7-sql
```

No se cambia la página, el esquema SQL, los valores de tbcmd, los períodos,
la agrupación por dispositivo ni el catálogo de alarmas. No se incorpora cola
persistente ni se promete recuperación tras reinicio. El motivo del primer
timeout del PLC real continúa pendiente de captura de tráfico o diagnóstico PLC.
