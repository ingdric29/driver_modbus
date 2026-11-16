# Propuesta de separación de responsabilidades

Estado: primera etapa implementada. Las etapas segunda y tercera permanecen
pendientes de selección y aprobación. No se cambia el esquema SQL, los
protocolos, las reglas de alarmas ni las opciones de configuración.

## Primera etapa: separación de loadconfig.cpp — implementada

El archivo se sustituyó por estos módulos, conservando nombres, parámetros,
retornos, bloqueos, consultas SQL y lógica de las funciones trasladadas:

| Archivo | Responsabilidad |
| --- | --- |
| `config_file.cpp` | Lectura, validación y resumen CFG: `loadconfigdb` |
| `device_catalog.cpp` | Catálogo de equipos: `actualizar_leer_equipo` y estado compartido |
| `action_catalog.cpp` | Alarmas, directorio, SMS y eventos |
| `command_repository.cpp` | Solicitudes, carga y confirmación de comandos |
| `sql_repository.cpp` | Inserciones, búsquedas y actualizaciones; helpers particulares privados |
| `replication.cpp` | Sincronización y replicación selectiva |
| `retention.cpp` | Borrado histórico con su política existente |
| `catalog_lifecycle.cpp` | `lee_tbcambios` y `liberarmemoria` |
| `repository_support.cpp` | Acceso, errores SQL y una única instancia del logger |

`repository_support.h` declara los helpers compartidos bajo `repository_detail`.
Las conexiones y transacciones siguen gestionadas por `sql_session.h`.
El makefile y el runner de estado enlazan los módulos nuevos. Las referencias
de archivo/línea de los diagnósticos reflejan ahora su ubicación real.

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
