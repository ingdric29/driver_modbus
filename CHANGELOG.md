# Historial de cambios

Este historial distingue funciones disponibles de propuestas pendientes.
La versión de producto sigue el formato MAYOR.MENOR.PARCHE. La versión 1 del
protocolo JSON del monitor es independiente de la versión del producto.

## Desarrollo de órdenes Modbus — 2026-11-21

- Medición monotónica por recorrido local/externo: bloqueo, SQL, ejecución y
  confirmación. Diagnóstico por ejecución/comando e intentos FC06; percentiles
  en ventana acotada y resumen al cierre.
- Prueba aislada SQL → Modbus con alarmas CMD/EMAIL, SMTP TLS, fallos Modbus y
  de confirmación SQL. Disponible con `bash install.sh --test-command-sql`.
- Alcance y límites: [informe de medición](docs/etapas/11_ordenes_metricas.md).
- Sondeo de órdenes con plazos monotónicos independientes, períodos fraccionarios,
  vencimiento exacto y salto de intervalos perdidos. El origen externo deshabilitado
  no gobierna la espera. Se conservan los períodos configurados y de adquisición.
- Pruebas de planificación y de órdenes locales/externas con alarmas/correo:
  [informe de sondeo](docs/etapas/12_ordenes_sondeo.md).

## 1.3.0 — 2026-11-17

Nombre de producto: AutoMonitor Driver; adquisición industrial multiprotocolo.
Versión sin sufijo RC ni de pruebas, por decisión del usuario. La rama actual se
renombra a automonitor-driver-V1.3.0 conservando historial. No se crea etiqueta
ni release automáticamente. Ejecutables automonitorv1/automonitorctl conservados.

- Siemens S7 por Snap7 fijado: lectura DB y ocho tipos, sesiones por PLC,
  reconexión, agrupación/PDU y adquisición simultánea con Modbus.
- Catálogo SQL con rack/slot, direccion_siemens y tipos s7_*; instantáneos,
  histórico, rastro, registrable/solocambios y reintentos acotados en RAM.
- Corrección de representación decimal REAL sin imponer decimales fijos.
- Instalador existente con prueba SQL mixta opcional; resumen de tags/PLC S7.
- Monitor, recarga y pruebas locales de fallos, persistencia y distribución.
- Usuario confirmó lectura física con variaciones y escritura SQL en Raspberry.
  La validación física de las últimas correcciones sigue pendiente.
- Alarmas/eventos Siemens, I/Q/M y cola persistente siguen pendientes.

## En desarrollo — 2026-11-16 — rama de pruebas

Cambios publicados en Git en `automonitor-modbus-driver-V1.2.0-pruebas`,
posteriores a la etiqueta existente `v1.2.0`. La versión del producto permanece
1.2.0; no se creó una nueva entrega etiquetada ni se integraron estos cambios
en la rama estable o en `main`.

### Limpieza y organización

- Retirados objetos y logs versionados, `main_old.cpp`, logo antiguo sin uso,
  binarios históricos, scripts de autoinicio y supervisor Orquesta.
- Corregidos warnings de variables sin uso, tipos, sangría y comprobaciones de
  direcciones. Se mantienen llamadas con efectos, bloqueos y alternativas Windows;
  el pragma de Microsoft se limita a `_MSC_VER`.
- Rechazo de ventanas RS232 negativas y tratamiento explícito del fallo de
  consulta SQL de órdenes; pruebas de búsquedas y tamaños extremos.
- Tres etapas de modularización completadas: repositorios/catálogos;
  coordinador/trabajadores; equipos/conversiones/alarmas. Se conservaron contratos,
  cuerpos de funciones y orden de inicialización de globales/estáticos durante
  sus movimientos. Makefile y runners enlazan los módulos separados.
- Formato propio unificado, encabezados de propósito/requisitos/retornos y
  contratos de funciones. Nombres personales retirados de encabezados propios;
  licencias de terceros y fixtures históricos conservados.

### Instalación y operación

- Nuevo `install.sh`: índices/dependencias APT, compilación como usuario normal,
  pruebas, verificación de versión, distribución y limpieza de objetos. Conserva
  el CFG; no inicia servicios ni crea el esquema SQL.
- Barra de ocho etapas completadas, con salidas visibles y error original
  conservado. El porcentaje no estima duración.
- Ejecutables generados en `bin/`, objetos en `.build/` y `make dist` para crear
  carpetas de ejecución sin fuentes ni configuración privada en `dist/`.
- Consola de arranque por bloques y métricas acumuladas legibles cada minuto/al
  cerrar. Los contadores técnicos completos permanecen en el log y el monitor.

### Verificación y pendientes

- Compilación Linux sin warnings de GCC y suite ordinaria aprobadas. Las tres
  etapas pasaron integración SQL local, recarga, cierre y correo simulado;
  distribución sin fuentes comprobada. Los cambios de comentarios se auditaron
  contra instrucciones/AST/comandos originales; la barra se verificó con
  comandos simulados, incluyendo fallos en todas las etapas.
- Windows, hardware industrial y despliegue final en Raspberry no se validaron
  en cloud. La instalación real de paquetes del nuevo script se verificará allí.
- Pendientes: seis decimales en persistencia/visualización, `configdb.example.cfg`
  y mejoras funcionales de retención, órdenes, TLS SMTP y aislamiento Modbus.

El [registro de jornada](docs/registro_2026-11-16.md) contiene los commits y
las comprobaciones de continuidad. El incidente de `tbejecutar` fue resuelto
por el usuario ajustando el concentrador del CFG a la base; no es un fallo activo.

## 1.2.0 — 2026-11-13

Primera versión unificada de la rama `mejoras-driver-pasos-1-2`, posterior a la
referencia documental histórica 1.1.4. Los antiguos banners V3.6/Rev. 1.1.2 no
representaban una numeración coherente de entregas.

### Incorporado en la rama

- Monitor SSH `automonitorctl`: dispositivos, tags, vigencia, alarmas, eventos
  y tasa de llamadas de lectura; modos de salida de texto y JSON.
- Reutilización y serialización de conexiones Modbus, espera progresiva de
  reconexión y agrupación de registros compatibles.
- Coordinación de trabajadores, ventanas de muestras y mejoras de persistencia.
- Compilación separada en `.build` y pruebas locales de Modbus, estado, correo,
  monitor e integración SQL opcional.
- Documentación de instalación, operación y diagnóstico.

### Correo

- Transferencia de contenido a libcurl respetando la capacidad del buffer y
  continuando por fragmentos; corrección de mensajes largos.
- Registros por alarma con resultado SMTP y tiempos, sin traza verbosa de AUTH.
- Error explícito si libcurl no se inicializa.
- Conservación de las opciones de envío heredadas y Message-ID por mensaje.
- Diagnóstico de conectividad/TLS sin autenticar ni enviar mensajes.
- Resolución confirmada en una Raspberry del rechazo 535: se corrigió únicamente
  `smtp_servidor` local para usar el servidor de la instalación funcional. No
  fue una modificación de credenciales ni una corrección de código.

### Identificación de esta entrega

- Versión única en `src/include/version.h`, usada por driver y visor.
- `automonitorv1 --version` y `automonitorctl --version`.
- Banner de arranque con versión, commit y fecha UTC de compilación mediante make.
- JSON del monitor con `driver_version`, `driver_commit` y `build_date`, sin
  alterar su campo `version: 1` de protocolo.

### Limitaciones y pendientes

- Tags/s cuenta llamadas de lectura, incluidos fallos y omisiones; no mide
  exclusivamente adquisición válida ni transacciones Modbus.
- Las lecturas siguen secuenciales por protocolo; la suspensión actual no
  garantiza aislamiento de todos los equipos/pasarelas.
- La validación de cadena TLS del emisor sigue desactivada por compatibilidad
  heredada. Restaurarla con CA apropiadas queda pendiente.
- Cola de correo, resultado agregado por destinatario, aislamiento mejorado,
  métricas de tags válidos/s y planificación por enlace son propuestas pendientes.
- El sitio PHP y el esquema de producción requieren distribución/configuración
  separada. Pruebas locales no sustituyen pruebas sobre equipos físicos.

## Histórico anterior

El README anterior citaba 1.1.4 y el ejecutable mostraba otros identificadores.
No se asignan retrospectivamente etiquetas ni fechas de entrega sin evidencia.
Consultar el historial Git para los cambios anteriores a 1.2.0.
