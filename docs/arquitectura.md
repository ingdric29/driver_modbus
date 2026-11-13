# Organización del driver

| Componente | Responsabilidad |
| --- | --- |
| `main.cpp` | Configuración inicial, coordinador, comandos y señales |
| `acquisition.cpp` | Vencimientos individuales y agrupación de tags ya vencidos |
| `persistence.cpp` | Snapshots, ventanas pendientes, reintento y confirmación SQL |
| `cls_equipos.cpp` | Lectura y publicación de valores TCP/RTU/RS232 |
| `modbus_transport.h` | Conexiones compartidas, exclusión por bus y recuperación |
| `modbus_decode.h` | Conversión explícita de palabras a enteros y vistas legacy |
| `sample_window.h` / `pending_batch.h` | Extremos adquiridos y confirmación de lotes |
| `sql_session.h` | Sesiones por hilo, identificadores y transacciones preparadas |
| `alarm_expression.h` | Validación y evaluación de expresiones |
| `worker_group.h` | Propiedad, parada, unión y propagación de fallos de trabajadores |

La configuración se reemplaza entre generaciones, después de unir los
trabajadores. El estado de muestras/contadores se protege por dispositivo;
el buffer RS232 se protege por separado. SQL recibe copias independientes.
Los comandos usan su propio bloqueo y las operaciones Modbus comparten el
bloqueo del endpoint/puerto. Los hilos no se desacoplan mediante detach.

Se mantienen los nombres del esquema existente, incluidos `html_id` y
`html_final` como delimitadores RS232. HTML/S9 ya no forma parte del driver.
No se modifican automáticamente los periodos ni se habilitan picos en tags
existentes. La adquisición sigue teniendo un trabajador por protocolo: una
operación lenta todavía puede retrasar otros destinos de ese protocolo.
