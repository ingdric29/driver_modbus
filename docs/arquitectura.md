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
| `config_file.cpp` | Lectura, validación y resumen del archivo CFG |
| `device_catalog.cpp` | Carga de equipos y asociación de su estado compartido |
| `action_catalog.cpp` | Catálogos de alarmas, correo, SMS y eventos |
| `command_repository.cpp` | Solicitudes SQL, carga de comandos y marca de confirmación |
| `sql_repository.cpp` | Wrappers de inserción, búsqueda y actualización |
| `replication.cpp` / `retention.cpp` | Replicación y limpieza histórica existentes |
| `catalog_lifecycle.cpp` | Detección de cambios y liberación de catálogos |
| `repository_support.cpp` | Helpers internos de acceso SQL y logger compartido |
| `alarm_expression.h` | Validación y evaluación de expresiones |
| `worker_group.h` | Propiedad, parada, unión y propagación de fallos de trabajadores |

La configuración se reemplaza entre generaciones, después de unir los
trabajadores. El estado de muestras/contadores se protege por dispositivo;
el buffer RS232 se protege por separado. SQL recibe copias independientes.
Los comandos usan su propio bloqueo y las operaciones Modbus comparten el
bloqueo del endpoint/puerto. Los hilos no se desacoplan mediante detach.

Los módulos de repositorio sustituyen a `loadconfig.cpp`. Conservan las firmas
públicas de `globales.h`; `repository_support.h` declara únicamente los helpers
compartidos internos. Los helpers de actualización y búsqueda de campos siguen
privados de `sql_repository.cpp`. Se mantiene un solo logger de repositorios y
la gestión de conexiones/transacciones sigue perteneciendo a `sql_session.h`.
La ubicación de archivo/línea en los diagnósticos corresponde al nuevo módulo.

Se mantienen los nombres del esquema existente, incluidos `html_id` y
`html_final` como delimitadores RS232. HTML/S9 ya no forma parte del driver.
No se modifican automáticamente los periodos ni se habilitan picos en tags
existentes. La adquisición sigue teniendo un trabajador por protocolo: una
operación lenta todavía puede retrasar otros destinos de ese protocolo.
