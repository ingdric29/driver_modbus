# Historial de cambios

Este historial distingue funciones disponibles de propuestas pendientes.
La versión de producto sigue el formato MAYOR.MENOR.PARCHE. La versión 1 del
protocolo JSON del monitor es independiente de la versión del producto.

## Sin publicar — rama de pruebas

- Instalador Linux reproducible: dependencias APT, compilación y pruebas como
  usuario normal, sin modificar configuración ni arrancar el driver.
- Retirados los tres scripts históricos de autoinicio; revisar referencias
  externas antes de actualizar instalaciones que dependan de ellos.

- Arranque de consola organizado en configuracion, intervalos y adquisicion;
  base externa deshabilitada resumida en una linea y estados explicitos.
- Metricas acumuladas legibles en consola cada minuto y al cerrar, conservando
  los contadores completos en el log y el monitor. Sin secuencias de terminal.
- Limpieza de variables y asignaciones sin uso, conservando las llamadas con
  efectos y el resultado de evaluación de alarmas.
- Pragma de Microsoft limitado a `_MSC_VER`; se conserva para Windows.
- Índices de cadenas/vectores y tamaños de archivo con tipos compatibles;
  rechazo de ventanas RS232 negativas antes de convertir su tamaño.
- Comprobaciones redundantes de direcciones de elementos retiradas; se
  conservan los controles de puntero base y límites del recorrido.
- Llaves explícitas en la comprobación de interfaces RTU/RS232 y retorno
  inmediato si falla la consulta SQL de órdenes.
- Pruebas de directorios nulos/vacíos, búsquedas y ventanas RS232 extremas.

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
