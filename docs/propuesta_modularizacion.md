# Propuesta de separación de responsabilidades

Estado: las tres etapas de separación están implementadas. Los cambios
funcionales adicionales requieren planificación y aprobación por separado. No se cambia el esquema SQL, los
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

## Segunda etapa: adelgazar main.cpp — implementada

| Archivo propuesto | Responsabilidad |
| --- | --- |
| `main.cpp` | Argumentos, versión, arranque, código de salida y definiciones globales heredadas |
| `driver_runtime.cpp` | Coordinador, carga/recarga y unión de trabajadores |
| `command_worker.cpp` | Sondeo y ejecución local/externa de órdenes |
| `maintenance_worker.cpp` | Programación de sincronización y retención |
| `serial_worker.cpp` | Lectura física RS232 y recuperación del puerto |
| `event_worker.cpp` | Actualización de referencias y valores de eventos |

Se añadieron también `alarm_worker.cpp` para la revisión de alarmas,
`catalog_lookup.cpp` para las búsquedas por ID y `runtime_support.cpp` para
mensajes, esperas y conversiones heredadas. No se introducen hilos nuevos ni
se alteran los períodos, mutexes, consultas SQL o reglas de alarma.

Los globales permanecen en `main.cpp` en el orden original de inicialización.
`driver_services.h` declara los servicios compartidos que necesitan los módulos;
las firmas de funciones de `globales.h` no cambian. La separación de estado o
introducción de un contexto requiere una propuesta posterior: puede afectar
inicialización, concurrencia y cierre.

## Tercera etapa: equipos, conversiones y alarmas — implementada

| Archivo | Responsabilidad |
| --- | --- |
| `cls_equipos.cpp` | Construcción, estado, errores, publicación de muestras y logger/estáticos originales |
| `equipment_tcp.cpp` | `cls_equipos::leer_mbtcp` |
| `equipment_rtu.cpp` | `cls_equipos::leer_mbserial` |
| `equipment_rs232.cpp` | `cls_equipos::leer_rs232`, parsing del buffer recibido |
| `conversiones.cpp` | Conversiones numéricas y helpers de valores |
| `modbus_conversions.cpp` | Conversiones heredadas de palabras Modbus a float |
| `text_conversions.cpp` | Separación, normalización y tratamiento de cadenas |
| `datetime_conversions.cpp` | Fechas y formatos de correo |
| `network_utils.cpp` | Ejecución de comandos y utilidades de interfaces/red existentes |
| `cls_defalarmas.cpp` | Constructor, resolución de tags y evaluación |
| `alarm_actions.cpp` | Ejecución de CMD/EMAIL/SMS con la lógica existente |
| `alarm_history.cpp` | Persistencia del histórico de alarmas |

Se conservan las firmas públicas, cuerpos y literales de las 56 funciones
involucradas, las alternativas de plataforma y el orden de inicialización de
estáticos de equipos. `equipment_internal.h` comparte una única instancia del
logger; no introduce estado nuevo. Los runners de estado y conversiones enlazan
los módulos nuevos. La ubicación de archivo/línea en los diagnósticos cambia
para reflejar el módulo que contiene ahora la función.

Esta etapa no cambia escalamiento, precisión SQL, algoritmos de conversión,
reintentos, destinatarios, políticas de borrado ni concurrencia por protocolo.
Las bibliotecas incorporadas y los fixtures históricos no se modifican.

## Comprobaciones para cada movimiento

1. Actualizar el makefile y los runners que enumeran fuentes explícitamente;
   el runner de estado enlaza `main.cpp` renombrando su punto de entrada,
   junto con los módulos de ejecución separados.
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
