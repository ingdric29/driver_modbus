# Corrección de contadores y recuperación Modbus

Fecha: 24 de noviembre de 2026 (Caracas).
Rama: `automonitor-driver-V1.3.0`. Base: `9649eff`.

## Problema observado y alcance

En la Raspberry un lote Ajuste cargaba 36 órdenes, intentaba las 36 y obtenía
35 aceptaciones. El ID 26 (registro 16404, valor 20) terminaba por timeout.
La invalidación del enlace compartido provocaba lecturas pospuestas por espera
de recuperación. El monitor sumaba esas omisiones a los errores de lectura por
tag, dando la impresión de muchas lecturas fallidas al escribir.

Además, una excepción Modbus válida se repetía hasta tres veces y terminaba
cerrando el enlace, aunque el equipo había respondido correctamente al protocolo.
No se atribuye un timeout a un registro inexistente sin comprobación del PLC.

## Cambios

- `E`: errores de adquisición intentada, incluidas excepciones/fallos de conexión.
  Las omisiones `-2` (suspensión) y `-3` (espera) pasan a `P` y no alteran los
  errores consecutivos ni el total de lecturas intentadas. Se conserva valor,
  fecha y edad, con calidad REINTENTANDO.
- `W`: llamadas al ejecutor con FC06 intentado y resultado final fallido; no
  aumenta por cada reintento interno ni por sondeos de recuperación sin envío.
- `R`: conexiones recuperadas exitosamente después de una sesión previa.
  Primera conexión e intentos fallidos se distinguen de reconexiones exitosas.
- `W`/`R` pertenecen al enlace físico; dispositivos del mismo TCP IP:puerto o
  puerto RTU muestran los mismos contadores. No sumar esos valores entre ellos.
- Excepciones válidas (códigos Modbus 1..11 reconocidos por libmodbus): se registra
  el fallo, termina el intento sin repetición automática ni cierre, y continúa
  la ejecución normal. ACK/busy no se convierten en aceptación FC06 ni en ACK SQL.
- Lecturas TCP/RTU siguen la misma distinción entre excepción válida y error
  de transporte. Prefetch conserva su fallback individual si un bloque falla.
- Timeouts/conexiones rotas/protocolo inválido mantienen invalidación y espera.
  Se conservan los tres intentos heredados para errores sin excepción válida y
  el presupuesto de 500 ms por lote para órdenes todavía no enviadas. No existe
  garantía de exactamente una ejecución física ante pérdida de respuesta.
- JSON versión 1 conserva sus campos y agrega contadores; el visor admite
  instantáneas anteriores usando cero en los campos nuevos.

Los contadores por tag se reinician al recargar catálogo; los del enlace al
limpiar enlaces después de detener trabajadores. Métricas generales viven durante
el proceso. Tags/s ahora excluye omisiones, pero incluye caché y fallos.

## Compatibilidad y alarmas

Sin migraciones SQL, cambios PHP, nuevos parámetros ni cambios en períodos.
No se modifica el ID 26, el catálogo de alarmas, la interpretación/escalamiento,
las transiciones de alarma, SMTP ni su política de reintento. Las acciones CMD de
alarma usan el transporte corregido y mantienen su política actual de evaluación;
esta corrección no implementa colas persistentes (etapas pendientes).
Snap7 conserva su comportamiento y se prueba la adquisición mixta.

## Verificación

Se ejecuta el instalador existente en el entorno aislado, sin instalar paquetes:

```bash
bash install.sh --skip-packages --test-command-sql --test-s7-sql
```

El entorno cloud utiliza las rutas de dependencias preparadas bajo
`/workspace/driver-modbus-env` mediante INCLUDES/LIBS y S7_TEST_SYSROOT. Las
pruebas usan servidores temporales loopback y pseudoterminales, no el PLC real.

Verificaciones añadidas:

- Monitor: fallo real seguido de suspensión/backoff conserva un solo error,
  mantiene errores consecutivos/último valor y cuenta omisiones aparte.
- Métricas generales: omisiones no aumentan lecturas intentadas ni fallos.
- Visor: presentación E/P/W/R y compatibilidad con JSON anterior.
- Driver enlazado TCP y RTU: escritura ilegal y lectura ilegal hacen una sola
  solicitud cada una; la siguiente operación válida usa la misma conexión.
- Integración SQL: tras timeout de ID 26 se ejecutan 56/57, con W=1, reconexión
  exitosa y E=0 si las lecturas responden; fechas de órdenes fallidas no cambian.
- Excepción de escritura: una solicitud, un fallo W, sin nueva conexión ni aumento
  de E; siguiente orden válida aceptada.
- Enlace caído, continuación por otro dispositivo y parada interrumpible.
- Regresión de alarmas CMD/EMAIL y correo real contra SMTP temporal TLS/AUTH,
  incluyendo activación/desactivación y perfiles SQL local/externo.

Resultado final: **PASS**, instalador con 10/10 pasos y salida 0. Pruebas
unitarias con ASan/UBSan, TCP/RTU enlazadas, monitor/TUI, Snap7/adquisición mixta,
SQL mixto y tres perfiles de órdenes (local, externo y recuperación) completados.
Los perfiles local y externo verificaron tres correos TLS/AUTH cada uno.

Evidencia cloud: `/workspace/driver-modbus-env/counters-installer-final.log`.
La regresión reproduce tres intentos por timeout de ID 26 y un único envío de
56/57; confirma W=1, R>=1, E=0 cuando las lecturas contestan. Después, la excepción
incrementa W una vez sin reconectar y sin incrementar E. Se verificó también
configdb.cfg sin cambios (SHA-256 igual al estado inicial).

Durante la validación se actualizaron las expectativas heredadas que contaban
3 solicitudes por excepción y se hizo esperar al test por el refresco del visor
(cache de 1 segundo). La ejecución completa final pasó después de esos ajustes.
No se modificó el cache ni la frecuencia de publicación del monitor.

## Validación pendiente en Raspberry

Instalar mediante install.sh y desplegar todo bin, incluido bin/lib. Reiniciar
el driver y actualizar también automonitorctl. Comprobar:

1. Escritura válida: E/W/R sin aumentos atribuibles a la operación.
2. ID 26 aún habilitado y con timeout: W aumenta una vez por llamada fallida y
   R al recuperar; P puede aumentar. E solo aumenta si falla una adquisición
   realmente intentada. No se espera eliminar el timeout con estos cambios.
3. Si se decide deshabilitar ID 26: verificar el lote sin ese timeout.
4. Confirmar ambas transiciones de alarmas y recepción de correos.

No se debe provocar una escritura inválida en producción solo para probarla.

## Herramientas y consulta

Git; install.sh/Make; compilador C++ con ASan/UBSan en las pruebas unitarias;
libmodbus; Python; MariaDB temporal; servidor SMTP TLS/AUTH de pruebas;
Snap7 y simuladores locales; pseudoterminales RTU; clang-format y git diff --check.

Después de actualizar y desplegar el binario y visor, desde la carpeta instalada:

```bash
./bin/automonitorctl --once
grep -aE 'command=26 |batch="Ajuste"' log/devlog.log | tail -n 10
```

La consulta del visor usa memoria: no genera solicitudes Modbus ni SQL.
