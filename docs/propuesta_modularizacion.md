# Propuesta de separación de responsabilidades

Estado: propuesta pendiente de selección y aprobación. No se han movido
funciones como parte de esta limpieza. No propone cambiar el esquema SQL,
los protocolos, las reglas de alarmas ni las opciones de configuración.

## Primera etapa: separar loadconfig.cpp

Es el archivo con más responsabilidades mezcladas. Empezaría por él, en cambios
pequeños que muevan funciones manteniendo sus nombres y contratos actuales.

| Archivo propuesto | Responsabilidad y funciones actuales |
| --- | --- |
| `config_file.cpp` | Lectura y validación del CFG: `loadconfigdb` |
| `device_catalog.cpp` | Catálogo de equipos: `actualizar_leer_equipo` y asociación de estado compartido |
| `action_catalog.cpp` | Carga de alarmas, directorio, SMS y eventos: `actualizar_defalarmas`, `actualizar_directorio`, `actualizar_smslist`, `actualizar_defeventos` |
| `command_repository.cpp` | Consulta de solicitudes, carga de comandos y confirmación: `leer_tbejecutar`, `leer_tbcmd`, `escribir_tbcmd_fechahora` |
| `sql_repository.cpp` | Wrappers de inserción, búsqueda y actualización de campos |
| `replication.cpp` | `sincronizar_tablas` y `replicaciontablaselectiva` |
| `retention.cpp` | `borrar_registros`, sin alterar todavía su política de borrado |

Los helpers del espacio anónimo de `loadconfig.cpp` se revisarían antes de
moverlos: los que sean compartidos necesitan una interfaz interna explícita;
los particulares deben permanecer privados del módulo que los utiliza.
`sql_session.h` seguiría gestionando conexiones/transacciones, sin duplicar
esa función en los repositorios nuevos.

No movería todo en un único cambio: primero configuración, después comandos,
luego replicación/retención y finalmente catálogos y helpers SQL. Revisaría en
cada paso que no cambien los vínculos entre tablas y objetos ni el ciclo de vida.

## Segunda etapa: adelgazar main.cpp

| Archivo propuesto | Responsabilidad |
| --- | --- |
| `main.cpp` | Argumentos, versión, arranque y código de salida |
| `driver_runtime.cpp` | Coordinador, carga/recarga y unión de trabajadores |
| `command_worker.cpp` | Sondeo y ejecución local/externa de órdenes |
| `maintenance_worker.cpp` | Programación de sincronización y retención |
| `serial_worker.cpp` | Lectura física RS232 y recuperación del puerto |
| `event_worker.cpp` | Actualización de referencias y valores de eventos |

Los globales no se convertirían automáticamente en una gran clase. Primero se
documentarían propietarios, lectores, escritores y bloqueos. Separar estado e
introducir un contexto explícito sería otro cambio posterior, porque puede
afectar inicialización, concurrencia y cierre.

## Tercera etapa: equipos, conversiones y alarmas

- `cls_equipos.cpp`: separar lectura TCP, lectura RTU y parsing RS232 de la
  publicación de muestras y el estado de comunicación. Conservar inicialmente
  los métodos públicos de la clase. No mezclar esta operación con una nueva
  planificación concurrente ni con la unificación de todos los decodificadores.
- `conversiones.cpp`: agrupar conversiones numéricas/texto, fechas e interfaces
  de red en módulos propios. Mantener las alternativas Windows/Linux.
- `cls_defalarmas.cpp`: separar evaluación de expresiones, ejecución de acciones
  y persistencia del histórico. No cambiar los reintentos ni los resultados de
  correo como efecto secundario del movimiento.

## Comprobaciones para cada movimiento

1. Actualizar el makefile y los runners que enumeran fuentes explícitamente;
   el runner de estado hoy enlaza `main.cpp` renombrando su punto de entrada.
2. Compilar y ejecutar `make test`; cuando se mueva código SQL o el coordinador,
   ejecutar también `make test-sql` con el perfil local reservado.
3. Verificar arranque, recarga, cierre y construcción de la distribución.
4. Mantener formato y cambios funcionales separados en la revisión. Añadir
   pruebas de caracterización si aparece una ruta relevante no cubierta.

Las pruebas Linux no validan por sí solas Windows ni equipos industriales
físicos. No eliminar utilidades por parecer antiguas sin comprobar sus llamadas,
sus alternativas de plataforma y su uso por las pruebas.

## Fuera de esta propuesta

`configdb.example.cfg` queda pendiente para el final, por indicación del usuario.
Retención segura, estados de órdenes, TLS SMTP y aislamiento de equipos siguen
siendo mejoras funcionales separadas; no se implementan al dividir archivos.
