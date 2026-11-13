# Automonitor / Devlog — Driver Modbus

Driver C++ para adquisición de variables industriales, evaluación de alarmas y
registro de datos. Soporta Modbus TCP, Modbus RTU y lectura serie RS232; utiliza
MySQL/MariaDB para configuración y persistencia, y libcurl para correo SMTP.

## Ramas y alcance

| Rama | Uso y diferencias |
|---|---|
| `main` | Versión histórica. Compila hacia `bin/automonitorv1`; incluye binarios y objetos ARM64 versionados. No incluye `automonitorctl` ni la suite de pruebas de la rama mejorada. |
| `mejoras-driver-pasos-1-2` | Desarrollo mejorado: compilación en `.build`, monitor SSH, reutilización de conexiones Modbus, agrupación de registros compatibles, métricas, pruebas y correcciones del emisor de correo. |

Los comandos de `.build`, monitor y pruebas de este documento corresponden a
`mejoras-driver-pasos-1-2`. No están disponibles en `main` sin incorporar esos
cambios. Antes de cambiar de rama, conserva cualquier modificación local:

```bash
git status --short
git branch --show-current
# Solo con el trabajo local guardado:
git switch mejoras-driver-pasos-1-2
```

## Requisitos y dependencias

El flujo de desarrollo documentado es Linux Debian/Raspberry Pi OS con C++11.
Los binarios deben compilarse para la arquitectura de destino: ARM64 y x86_64
no son intercambiables. La documentación histórica menciona Windows, pero
estos comandos y el monitor SSH se documentan para Linux.

En Debian/Raspberry Pi OS con permisos de administración:

```bash
sudo apt-get update
sudo apt-get install -y build-essential libmodbus-dev libmysqlcppconn-dev \
  libcurl4-openssl-dev libboost-filesystem-dev libboost-system-dev \
  python3 openssl ca-certificates
```

- `build-essential`: compilador C++ y make.
- `libmodbus-dev`: comunicación TCP/RTU.
- `libmysqlcppconn-dev`: conector MySQL C++ usado por el driver.
- `libcurl4-openssl-dev`: transporte SMTP.
- Boost.Filesystem y Boost.System: rutas y archivos.
- Python 3 con `curses`: visor de terminal y herramientas/pruebas.
- OpenSSL: generación de certificados locales para pruebas de correo.
- Certificados CA: diagnóstico TLS y validación de certificados.

Para una base local, instalar además `mariadb-server mariadb-client`. Para
operación serie, el usuario debe tener acceso al dispositivo, normalmente
mediante el grupo `dialout` (requiere una nueva sesión tras cambiar grupos).
Apache, PHP y phpMyAdmin no son requisitos para compilar el driver ni usar el
monitor; solo corresponden a un despliegue web separado.

No se distribuyen aquí el esquema SQL de producción ni el sitio PHP descrito
en documentación histórica. El driver completo necesita su configuración y
esquema de datos compatibles; este README no pretende reconstruirlos.

## Compilación

En la rama mejorada, desde la raíz del repositorio:

```bash
make -j2
# Supervisor opcional:
make orquesta
```

Se generan `.build/automonitorv1`, `.build/automonitorctl` y
`.build/automonitor-mailcheck`; el supervisor se genera en `.build/orquesta`.
La compilación mejorada conserva los binarios históricos y detecta cambios de
cabeceras. Para reconstruir desde las fuentes:

```bash
make clean
make -j2
```

`main` usa un makefile histórico que escribe sobre archivos versionados y puede
reutilizar objetos ARM64. Para una compilación nativa sin sobrescribirlos:

```bash
mkdir -p /tmp/automonitor-build
make -B TARGET=/tmp/automonitor-build/automonitorv1 'OBJS=$(SRCS)' \
  'INCLUDES=-Isrc/include -I/usr/include/modbus -I/usr/include/cppconn -I/usr/include/mysql'
```

No ejecutar `make clean` de `main` como si fuera el de la rama mejorada. No usar
`sudo make`: la compilación debe pertenecer al usuario de desarrollo.

## Configuración y arranque

**La ruta depende del directorio de trabajo, no de la ubicación del ejecutable.**
Actualmente el driver busca en el padre del directorio de trabajo:

- `configdb.cfg`: configuración inicial.
- `html/plantilla_emails.html`: plantilla del correo.
- `log/`: registros de operación.

En la rama mejorada, iniciar así desde la raíz del proyecto:

```bash
cd .build
./automonitorv1
```

Usar una configuración local autorizada y proteger sus credenciales. No publicar
contraseñas, archivos completos de configuración ni trazas de autenticación.
Los valores históricos incluidos en el repositorio son ejemplos heredados y
no deben asumirse válidos para otra instalación.

La configuración SMTP se carga al iniciar: editarla exige reiniciar de forma
controlada la instancia correspondiente. Evitar procesos duplicados; no arrancar
otro driver para consultar su estado. Los scripts históricos de autoinicio
apuntan a `bin` y deben revisarse antes de desplegar `.build`. `autoiniciar.sh`
contiene paradas globales de procesos y un volcado de variables de entorno;
no ejecutarlo sin adaptar ese comportamiento al despliegue.

El `install.sh` histórico es interactivo, modifica configuración y referencia
archivos ausentes. Se recomienda la instalación explícita de dependencias de
este README en lugar de ejecutarlo directamente.

En la rama mejorada, SIGINT/SIGTERM solicitan parada ordenada. Las operaciones
de red en curso pueden retrasarla, especialmente SMTP, que conserva los tiempos
predeterminados de libcurl sin un límite total añadido. No se garantiza la
persistencia de muestras que todavía estén pendientes en memoria.

## Monitor SSH: automonitorctl

Con el driver mejorado ya iniciado, abrir otra terminal en la raíz del proyecto:

```bash
./.build/automonitorctl
./.build/automonitorctl --once
./.build/automonitorctl --json
./.build/automonitorctl --device NOMBRE_DEL_EQUIPO
```

El visor consulta un socket local: no inicia adquisición ni hace consultas
Modbus adicionales. `--socket RUTA` permite indicar otro socket. Las flechas
seleccionan dispositivos/tags, Enter abre el detalle, Esc vuelve, R actualiza
y Q cierra únicamente el visor. Muestra estados, valores, períodos, antigüedad,
errores, alarmas y eventos.

### Qué significa «Lecturas tags/s»

Es el incremento del contador de llamadas completadas a lectura de tags dividido
por los segundos entre instantáneas del monitor. Incluye llamadas exitosas,
fallidas y omitidas por suspensión/espera de reconexión. No mide exclusivamente
datos válidos ni peticiones físicas Modbus.

- Un tag es una variable; leer el mismo valor otra vez también cuenta.
- Los reintentos internos no cuentan como llamadas independientes del tag.
- Un bloque Modbus puede abastecer varios tags y contabilizar varias lecturas.
- Omitir rápidamente equipos suspendidos puede elevar la cifra sin adquirir datos.
- El período configurado condiciona la tasa: 100 tags cada 10 s representan
  aproximadamente 10 lecturas/s de promedio si se cumplen sus períodos.

Evaluar esta tasa junto con errores, antigüedad y vigencia de los tags. La tasa
se reinicia visualmente al detectar otro proceso/generación. «Tags válidos/s»,
«peticiones Modbus/s» y cumplimiento agregado del período son mejoras propuestas,
no indicadores ya implementados con esos nombres.

## Correo: configuración y problema resuelto

Los campos SMTP son `smtp_servidor`, `smtp_usuario`, `smtp_pass` y `smtp_from`.
Usar el servidor y modo TLS que corresponden a la cuenta y al proveedor. SMTPS
suele usar TLS implícito en 465; STARTTLS es otro modo y no debe suponerse
intercambiable cambiando únicamente el puerto.

Durante el diagnóstico de una instalación Raspberry, la versión antigua enviaba
y la nueva devolvía `codigo=67 smtp=535 etapa=AUTH`. Se comprobó lo siguiente:

1. Ambas instalaciones leían archivos `configdb.cfg` distintos.
2. Usuario, contraseña y remitente coincidían; `smtp_servidor` difería.
3. Se respaldó la configuración nueva y se copió únicamente el servidor de la
   instalación que funcionaba. Después se reinició el driver nuevo.
4. El servidor respondió `codigo=0 smtp=250` y el usuario confirmó recepción.

**La solución fue corregir el servidor SMTP; no modificar el código ni las
credenciales.** No copiar una URL universal: conservar la válida para cada cuenta.
Los hashes distintos de archivos completos no prueban por sí solos diferencias
en credenciales; comparar los campos efectivos sin imprimir secretos.

La rama mejorada conserva una corrección importante de `payload_source`: respeta
el tamaño del buffer de libcurl y continúa los mensajes largos por fragmentos.
Evita escrituras fuera de memoria sin cambiar el contenido previsto. También
incorpora contexto por alarma, códigos y tiempos SMTP, desactiva la traza verbosa
y devuelve error si libcurl no se inicializa. Se conserva el Message-ID generado
por mensaje. El envío real confirmado funcionó con estas mejoras.

La política TLS heredada desactiva la verificación de cadena del certificado;
no debe confundirse cifrado con validación completa del servidor. Configurar CA
y restaurar esa verificación es una mejora pendiente, no una solución al error
535. No desactivar más verificaciones para resolver problemas de autenticación.

Diagnóstico sin autenticar ni enviar mensajes, en la rama mejorada:

```bash
./.build/automonitor-mailcheck --config /ruta/configdb.cfg
./.build/automonitor-mailcheck --config /ruta/configdb.cfg --ports 465,587
```

La herramienta valida conectividad/TLS, no credenciales ni entrega. No cambia
la configuración. Un SMTP 250 con resultado cURL 0 indica aceptación por el
servidor; comprobar recepción aparte, incluido spam cuando corresponda.

## Modbus TCP/RTU y equipos que no responden

Ambas ramas incluyen contador de errores, suspensión temporal y comprobación
automática posterior. Los parámetros heredados son `maxima_cuenta_errores`
(predeterminado 50) y `tiempo_restablecimiento_comunicacion` (60 segundos).
La suspensión se activa al superar el máximo; al reanudar, el contador se reduce
a la mitad. Los contadores se agrupan por nombre de equipo y una lectura exitosa
los reinicia: no equivalen estrictamente a peticiones físicas consecutivas.

La rama mejorada añade reutilización de conexiones, exclusión mutua por enlace,
lecturas agrupadas compatibles y espera progresiva de reconexión de hasta 30 s.
`modbus_timeout_us` establece el timeout de respuesta (por defecto 90000 µs,
rango admitido 1000–3000000); no garantiza ese límite para conectar por TCP.
El lector mantiene hasta tres intentos por lectura.

Las consultas siguen siendo secuenciales dentro de un hilo por protocolo.
Un equipo sin respuesta puede retrasar a otros por esperas y reintentos; las
lecturas anticipadas por bloques también consumen tiempo. En TCP el enlace se
comparte por IP/puerto, y en RTU por puerto serie: los esclavos de una pasarela
pueden compartir el impacto. No se garantiza aislamiento completo.

Mejoras propuestas, todavía pendientes de implementación y validación:

- Suspensión por identidad física (enlace y esclavo), con una sola consulta de
  recuperación y sin lecturas anticipadas durante la suspensión.
- Separar fallo del enlace de fallo de un esclavo y de excepciones por registro.
- Planificar conexiones TCP y puertos RTU independientes sin bloquearse entre sí.
- Timeouts y presupuestos de reintentos adecuados a cada equipo/bus.
- Adaptar bloques rechazados y comprobar respuestas tardías en RTU.

En RTU debe mantenerse una sola transacción activa por bus. Una suspensión por
software no corrige fallos eléctricos, alimentación, terminación o interferencias.
La aceptación de estas mejoras debe probar desconexión y recuperación de un
esclavo mientras los demás mantienen sus períodos, también detrás de pasarelas.

## Solución de problemas

| Síntoma | Comprobaciones |
|---|---|
| `Exec format error` | Revisar `uname -m` y `file RUTA_BINARIO`; recompilar para la arquitectura local. |
| Falta `ini.h`, `modbus.h` o bibliotecas al enlazar | Instalar paquetes `-dev` y comprobar includes; en `main` usar la compilación explícita anterior. |
| No carga configuración o plantilla | Revisar `pwd -P` y el padre del directorio de trabajo; no basta la ruta del ejecutable. |
| `DuplicateOptionError` en una herramienta Python | Se detectó `sicronizacion_periodo` repetida en ambas configuraciones analizadas. No era una clave SMTP. El parser del driver puede concatenar repeticiones; revisar sin borrar opciones a ciegas. |
| `67 / 535 / AUTH` | Comparar servidor y configuración efectiva con la instalación funcional. El fallo ocurre antes de enviar el contenido; plantilla y Message-ID no explican ese rechazo. |
| cURL 28 o 35 | Revisar tiempos, conectividad y negociación TLS; no demuestran contraseña incorrecta. |
| Monitor sin conexión | Confirmar driver mejorado iniciado, socket correcto y permisos. Abrir el visor no inicia el driver. |
| Tags/s alto con datos antiguos | Revisar errores y suspensiones; la tasa incluye lecturas sin adquisición válida. |
| PLC lento cuando otro equipo falla | Revisar topología, tiempos y reintentos compartidos; ver limitaciones Modbus anteriores. |
| RTU sin respuesta | Revisar permisos, puerto, esclavo, baudrate, paridad, bits, cableado y uso exclusivo del puerto. No compartir interfaz con RS232 directo. |

En `main` se detectaron desplazamientos de 32/48 bits sobre enteros de ancho
insuficiente y funciones no `void` sin retorno. Durante el arranque sin licencia
se confirmó SIGILL en la ruta que muestra interfaces: el binario contenía `ud2`
al terminar `imprimirmac_from_ifrname`. Son defectos de código; no atribuirlos a
SMTP. `main` conserva comprobación de licencia; la rama mejorada elimina esa
comprobación de arranque e ignora `clave1`/`clave2`. No confundir ambas versiones.

## Pruebas y límites de validación

En la rama mejorada:

```bash
make test
# Solo con una base de pruebas preparada según tests/README.md:
make test-sql
```

La suite incluye pruebas unitarias, estado, Modbus TCP local/RTU con
pseudoterminales, correo contra un servidor local, monitor y arranque.
`make test-sql` incluye integración con bases reservadas de prueba y el recorrido
completo de alarmas hasta SMTP local. No apuntar las pruebas a producción.
Pasar pruebas locales no sustituye validar dispositivos físicos ni el proveedor
SMTP real. No se ejecutaron estas suites como parte de esta actualización de
README. El envío real descrito arriba fue confirmado por el usuario.

En onboarding cloud se verificaron compilación nativa de ambos ejecutables,
operaciones SQL mediante el conector C++, intercambio Modbus local, inicialización
de curl y Boost. Las dependencias se prepararon sin sudo fuera del checkout en
`/workspace/devlog-env`; esa ruta es específica de aquella sesión, no un requisito
del proyecto. Usar el checkout cloud existente, sin crear worktrees salvo petición.

La rama mejorada amplía la documentación en `docs/` (correo, monitor, operación,
desarrollo y arquitectura) y `tests/README.md`. Esos archivos no existen en `main`.

## Créditos

Desarrollo original: Ing. Alejandro Diomede / Automatroni.
Modificaciones históricas y depuración: Ing. Javier Flores.
