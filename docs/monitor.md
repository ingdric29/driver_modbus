# Monitor SSH de Automonitor (Linux / Raspberry Pi)

`automonitorctl` muestra dispositivos, valores, alarmas, eventos y persistencia
mientras el driver sigue adquiriendo datos. Redibuja la misma pantalla cada
segundo. Es un cliente independiente: abrirlo o cerrarlo no inicia ni detiene
la adquisición. Pueden conectarse varios visores con el mismo usuario Linux.

## Actualizar e iniciar

Desde `~/driver_modbus`:

```bash
git pull --ff-only origin automonitor-modbus-driver-V1.2.0-pruebas
make -j2
```

Se generan `bin/automonitorv1` y `bin/automonitorctl`. El visor requiere
Python 3 con el módulo estándar `curses` (normalmente incluido en Raspberry Pi OS).
No requiere instalar ncurses para C++, un servidor web ni cambiar el esquema SQL.

La primera instalación requiere una parada controlada y un arranque del driver
actualizado para habilitar el socket. Detener la instancia existente con su
mecanismo habitual (SIGINT/SIGTERM), esperar su cierre y arrancar **una sola**
instancia con la configuración habitual. Por ejemplo, en la primera sesión SSH:

```bash
cd ~/driver_modbus/bin
./automonitorv1
```

En otra sesión SSH, con el mismo usuario que ejecuta el driver:

```bash
cd ~/driver_modbus
./bin/automonitorctl
```

El driver puede seguir ejecutándose con el servicio que ya utilices.
Ese servicio debe apuntar al binario actualizado y conservar el directorio de
trabajo habitual (`bin`, cuyo padre contiene `configdb.cfg`). Los
scripts históricos del repositorio no se modifican ni se ejecutan automáticamente.

El visor ocupa su propia terminal; la consola del proceso principal sigue
escribiendo su diagnóstico habitual. En ella se corrigen las etiquetas de log
para que INFO y METRICS no aparezcan como «Error registrado». Los cuerpos y
las direcciones de correo dejan de volcarse en pantalla; los fallos SMTP se
registran como eventos de CORREO con el motivo y el código de cURL.

## Controles

| Tecla | Acción |
|---|---|
| Arriba / abajo | Seleccionar dispositivo o tag en el detalle |
| Enter | Abrir/cerrar detalle de tags |
| Esc | Volver a la lista de dispositivos |
| Izquierda / derecha | Cambiar dispositivo, también dentro del detalle |
| PgUp / PgDn | Avanzar una página |
| R | Volver a leer el estado publicado en memoria |
| Q o Ctrl+C | Cerrar solamente el visor |

La selección se conserva por identidad del dispositivo durante las recargas.
El detalle muestra nombre, valor/unidad, calidad, período, edad, tiempo de
procesamiento y errores. La línea del tag seleccionado indica la hora de la
última lectura válida, su resultado (`rc`) y los fallos consecutivos.
Ampliar la terminal permite ver más columnas y más dispositivos. El mínimo es
55 columnas por 12 filas; se recomienda 100×24 o más.

Para consultar un dispositivo directamente:

```bash
./bin/automonitorctl --device 'NombreDelDispositivo'
```

Para un script, una terminal sin TUI o una captura puntual:

```bash
./bin/automonitorctl --once
./bin/automonitorctl --once --device 'NombreDelDispositivo'
./bin/automonitorctl --json
```

`--device` admite un nombre exacto o un fragmento único. Si dos equipos tienen
el mismo nombre, `--json` incluye su `key` completa para seleccionarlos.

## Interpretación del estado

El visor recibe resultados reales publicados por los hilos del driver. No
consulta Modbus ni SQL y no cambia los períodos de lectura/escritura existentes.
Los tags lentos y rápidos conservan sus períodos individuales.

| Estado del dispositivo | Significado |
|---|---|
| PENDIENTE | Todavía no hay intentos publicados en esta generación |
| ONLINE | Todos sus tags tienen una lectura válida y vigente |
| DEGRADADO | Solo parte de los tags tiene datos válidos y vigentes |
| SIN_RESPUESTA | No hay tags vigentes y existen lecturas fallidas |
| REINTENTANDO | Sin tags vigentes; hay lecturas suspendidas o espera de conexión |
| DATOS_ANTIGUOS | Sin tags vigentes; hay lecturas válidas que ya vencieron |

Cada tag es ACTUAL, PENDIENTE, ERROR, REINTENTANDO o ANTIGUO. Una lectura válida
se considera antigua cuando su edad supera `max(1 segundo, 3 × período del tag)`.
La edad usa un reloj monótono, incluyendo el momento de adquisición del bloque
Modbus si la lectura procede de caché. Si falla un intento, se conserva el
último valor válido con su edad y la calidad cambia inmediatamente. Una conexión
TCP abierta por sí sola no basta para mostrar ONLINE.

Los contadores distinguen:

- `E`: fallos de lectura de tags al intentar adquirir/conectar; incluye excepciones
  Modbus y fallos de conexión, no las esperas sin intento. No cuenta paquetes ni
  transacciones: una lectura puede usar caché o los reintentos existentes.
- `P`: lecturas pospuestas (`rc=-2`, suspensión; `rc=-3`, espera de recuperación).
  No incrementan `E`, las lecturas intentadas ni los errores consecutivos. Conservan
  el último valor y su edad; muestran REINTENTANDO, nunca ACTUAL por una omisión.
- `W`: escrituras intentadas con resultado final fallido, una por llamada al
  ejecutor, aunque haya hasta tres intentos de transporte. No incluye validación,
  confirmación SQL ni órdenes que no llegaron a llamar FC06.
- `R`: reconexiones exitosas después de una conexión previa. La primera conexión
  y los intentos fallidos de conexión no incrementan `R`.

`E`/`P` pertenecen a los tags y se reinician al recargar el catálogo. `W`/`R`
pertenecen al enlace físico TCP IP:puerto o RTU interfaz y se reinician al limpiar
el registro de enlaces tras detener los trabajadores. Si varios dispositivos
comparten un enlace, muestran el mismo `W`/`R`: no deben sumarse entre ellos.
JSON agrega `deferred_reads` en métricas, dispositivos y tags; por dispositivo
agrega `link_key`, `write_failures`, `reconnections` y `connection_attempts`.
Los campos anteriores y el protocolo JSON versión 1 se conservan.

`Lecturas tags/s` deriva ahora de lecturas no pospuestas; incluye fallos y uso de
caché, no mide exclusivamente muestras válidas ni transacciones Modbus. `TAG.ms`
mide la ejecución de `leer()`; si se usa un bloque ya adquirido, no incluye la
transacción previa del bloque. No representa un RTT de red. Las métricas generales
conservan sus contadores durante toda la vida del proceso.

Una excepción Modbus válida termina ese intento de lectura/escritura sin repetirlo
ni cerrar el enlace. Se conserva el error y no se confirma la orden en SQL. Un
código de reconocimiento/ocupado tampoco se trata como escritura confirmada.
Timeouts, desconexiones y errores de protocolo conservan la invalidación y espera
existentes, incluyendo los tres intentos heredados: no se agrega una política
nueva de reenvío de escrituras inciertas. Una orden que responde por timeout puede
haber sido aplicada por el PLC. Las reglas existentes de repetición de acciones
de alarma no cambian.

MariaDB indica el resultado de la última operación local observada (operaciones
comunes de consulta/actualización/inserción y transacciones históricas), junto
con su edad. SIN_DATOS indica que aún no se observó una operación; OK no se
infiere de una conexión abierta. No se envían consultas de comprobación desde
el visor. Un fallo SMTP se muestra separado de Modbus y MariaDB.

Las alarmas activas proceden de la última evaluación del driver. «Sin evaluar»
cuenta las que aún no se pudieron evaluar en esta generación; una alarma activa
con evaluación posterior fallida sigue contándose como activa hasta que el
driver la desactive. Las transiciones incluyen dispositivo y descripción en
los eventos. Se retienen hasta 128 eventos recientes en memoria; las repeticiones
consecutivas se agrupan. METRICS tiene sus propios contadores y no llena esa lista.
El historial completo de mensajes del logger permanece en `log/devlog.log`,
con su rotación habitual; los cambios de conexión también se publican en el visor.

Si se pierde la conexión con el proceso, el visor muestra **SIN CONEXION** y
marca la instantánea anterior como datos sin vigencia. Reintenta automáticamente;
no sustituye datos ausentes por estados simulados. Durante una recarga se muestra
RECARGANDO/CARGANDO, y el catálogo nuevo comienza en PENDIENTE.

## Socket local y límites

El driver crea `.runtime/automonitor.sock` en el padre de su directorio de trabajo.
La carpeta debe pertenecer al usuario del driver y tener permisos 0700; el socket
usa 0600. No abre puertos TCP de administración ni exporta credenciales de
configuración. El bloqueo de `.runtime/automonitor.lock` impide arrancar dos
instancias en el mismo directorio; un socket abandonado se recupera al arrancar.

Si instalaste el ejecutable en otra ubicación o usas un directorio de trabajo
distinto, especifica la ruta del socket:

```bash
./bin/automonitorctl --socket /ruta/al/proyecto/.runtime/automonitor.sock
```

El endpoint envía una instantánea JSON versión 1 y cierra; no acepta comandos.
Comparte una caché de hasta un segundo entre clientes. Un cliente lento dispone
de 200 ms de envío en el hilo del servidor; nunca mantiene bloqueos de adquisición
ni de SQL durante el envío. Las instantáneas tienen un límite de 8 MiB y los
valores de texto publicados se limitan a 256 bytes. El visor limpia caracteres
de control antes de mostrarlos en la terminal. El socket se elimina al finalizar
el proceso; el archivo de bloqueo puede permanecer y se reutiliza.
