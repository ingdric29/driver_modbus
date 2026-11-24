# AutoMonitor Driver — Adquisición industrial multiprotocolo

[Informe de jornada — 17 de noviembre de 2026](docs/registro_2026-11-17.md).

**Versión: 1.3.0.** Ver [CHANGELOG.md](CHANGELOG.md).

Driver C++ para adquisición de variables industriales, evaluación de alarmas y
registro de datos. Soporta Siemens S7, Modbus TCP, Modbus RTU y lectura serie RS232; utiliza
MySQL/MariaDB para configuración y persistencia, y libcurl para correo SMTP.

## Cierre de jornada — 16 de noviembre de 2026

Trabajo realizado en `automonitor-driver-V1.3.0`:
limpieza, corrección de warnings, consola de arranque, nuevo instalador con
barra de progreso, ejecutables en `bin/`, distribución sin fuentes y las tres
etapas de modularización. El código propio incluye encabezados y contratos
comentados. Orquesta y los scripts antiguos de autoinicio fueron retirados.

La versión de producto es **1.3.0**. La rama estable y `main` no incorporan todavía
estos cambios. Las comprobaciones cloud pasaron en Linux con recursos simulados;
la validación de operación en la Raspberry corresponde a la instalación local.

- [Registro completo y commits de hoy](docs/registro_2026-11-16.md).
- [Historial de cambios](CHANGELOG.md).
- [Mapa de arquitectura](docs/arquitectura.md) y [etapas completadas](docs/propuesta_modularizacion.md).
- [Instalación, validación local y pendientes](docs/operacion.md).

Siguen pendientes el límite de seis decimales y `configdb.example.cfg`.
No ejecutar pruebas SQL de integración sobre producción.

## Ramas y alcance

La incorporación Siemens S7 permite adquisición local de DB sin SQL:
[configuración, estado y pruebas](docs/siemens_s7.md). Snap7 se incluye en la
distribución. El arranque habitual también admite catálogo y persistencia SQL Siemens;
ver [informe de integración SQL](docs/etapas/07_sql_siemens.md).

| Rama | Uso y diferencias |
|---|---|
| `main` | Versión histórica. Compila hacia `bin/automonitorv1`; incluye binarios y objetos ARM64 versionados. No incluye `automonitorctl` ni la suite de pruebas de la rama mejorada. |
| `automonitor-driver-V1.3.0` | Desarrollo mejorado: ejecutables en `bin` y objetos en `.build`, monitor SSH, reutilización de conexiones Modbus, agrupación de registros compatibles, métricas, pruebas y correcciones del emisor de correo. |

Los comandos de `bin`, monitor y pruebas de este documento corresponden a
`automonitor-driver-V1.3.0`. No están disponibles en `main` sin incorporar esos
cambios. Antes de cambiar de rama, conserva cualquier modificación local:

```bash
git status --short
git branch --show-current
# Solo con el trabajo local guardado:
git switch automonitor-driver-V1.3.0
```

## Guía paso a paso en Raspberry: instalación, pruebas y arranque

Esta guía corresponde a `automonitor-driver-V1.3.0`. Ejecutar cada paso en orden;
si un comando falla, detenerse y revisar su salida antes de continuar.

### 1. Preparar la instalación o actualizar una existente

En una instalación existente, detener primero la instancia del driver mediante
su mecanismo habitual. Si está en primer plano, usar Ctrl+C. Detener también el
servicio de autoinicio para que no vuelva a lanzarlo durante las
pruebas. No iniciar una segunda instancia sobre los mismos equipos.

Respaldar la configuración local antes de actualizar:

```bash
cd ~/driver_modbus
cp -p configdb.cfg "$HOME/configdb.cfg.backup-$(date +%Y%m%d-%H%M%S)"
git status --short
git fetch origin --tags
git switch automonitor-driver-V1.3.0
git pull --ff-only origin automonitor-driver-V1.3.0
```

Si Git informa cambios incompatibles o conflictos, detenerse y conservarlos.
No usar `reset --hard`, forzar el cambio de rama ni descartar `configdb.cfg`.
El archivo local contiene el servidor SMTP que funciona y los ajustes del sitio.
La copia de seguridad contiene credenciales: conservarla privada.

Para una instalación nueva, instalar Git si hace falta y clonar la rama:

```bash
sudo apt-get update
sudo apt-get install -y git
git clone --branch automonitor-driver-V1.3.0 \
  https://github.com/ingdric29/driver_modbus.git ~/driver_modbus
cd ~/driver_modbus
```

Preparar `configdb.cfg` con los valores autorizados de la instalación y disponer
del esquema de datos compatible y la plantilla HTML antes del arranque real.
No asumir que las credenciales o destinos versionados sirven para otro sitio.

Comprobar qué revisión se descargó:

```bash
git branch --show-current
git log -1 --oneline
git describe --tags --always
```

La etiqueta `v1.2.0` identifica el commit de entrega `73c468c`. La punta de la
rama puede incluir documentación o correcciones posteriores; en ese caso
`git describe` puede mostrar `v1.2.0-N-gHASH`, sin indicar un error.

### 2. Instalar dependencias

```bash
sudo apt-get update
sudo apt-get install -y \
  build-essential libmodbus-dev libmysqlcppconn-dev \
  libcurl4-openssl-dev libboost-filesystem-dev libboost-system-dev \
  python3 openssl ca-certificates
```

Si se usa MariaDB en la Raspberry, instalar además `mariadb-server mariadb-client`
y preparar el servicio y el esquema de la instalación. En RTU, comprobar permisos
del usuario sobre el puerto serie y su configuración. No se necesita Apache/PHP
para el driver ni para el monitor SSH.

### 3. Compilar y probar

```bash
cd ~/driver_modbus
make clean && make -j2
```

Solo si termina correctamente, comprobar la identificación y ejecutar la suite:

```bash
./bin/automonitorv1 --version
./bin/automonitorctl --version
make test
```

En esta entrega ambos deben mostrar la versión de producto `1.3.0`; el driver
muestra también commit y fecha UTC de compilación. Compilar como usuario normal,
sin `sudo make`. La suite usa servidores locales y pseudoterminales: valida
software, pero no sustituye las pruebas con equipos físicos.
`make test-sql` es opcional y exige preparar las bases reservadas descritas en
`tests/README.md`; no ejecutarlo contra producción.

### 4. Arrancar el driver y abrir el monitor

Después de resolver cualquier fallo de las pruebas, arrancar en primer plano:

```bash
cd ~/driver_modbus/bin
./automonitorv1
```

Mantener esa terminal abierta. El directorio de trabajo debe ser `bin`, porque
el driver busca configuración, plantilla y logs en su padre. En otra sesión SSH:

```bash
cd ~/driver_modbus
./bin/automonitorctl
```

Para una consulta puntual sin interfaz interactiva:

```bash
./bin/automonitorctl --once
```

Verificar comunicación, actualización y antigüedad de tags, errores y alarmas.
La tasa tags/s incluye fallos y caché, pero excluye lecturas pospuestas; comprobar
también la vigencia del dato. El monitor separa E (fallos de lectura), P (pospuestas),
W (escrituras fallidas por enlace) y R (reconexiones); ver [detalle](docs/monitor.md).
Probar una notificación mediante una alarma prevista o una condición de prueba
segura: el envío debe registrar `codigo=0 smtp=250` y confirmar recepción aparte.
No provocar cambios peligrosos en el proceso industrial para probar alarmas.

### 5. Volver a operación supervisada

Detener la instancia de prueba con Ctrl+C antes de reactivar el servicio habitual.
Configurar su ejecutable como `~/driver_modbus/bin/automonitorv1` y su directorio
de trabajo como `~/driver_modbus/bin` (usar rutas absolutas expandidas en la
configuración del servicio). No se instala ni habilita automáticamente un servicio con
estos comandos; adaptar el servicio existente y comprobar que solo haya una
instancia del driver.

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
```

Se generan `bin/automonitorv1`, `bin/automonitorctl` y
`bin/automonitor-mailcheck`. El supervisor Orquesta fue retirado.
Los binarios generados no se versionan. La compilación detecta cambios de
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
cd bin
./automonitorv1
```

Usar una configuración local autorizada y proteger sus credenciales. No publicar
contraseñas, archivos completos de configuración ni trazas de autenticación.
Los valores históricos incluidos en el repositorio son ejemplos heredados y
no deben asumirse válidos para otra instalación.

La configuración SMTP se carga al iniciar: editarla exige reiniciar de forma
controlada la instancia correspondiente. Evitar procesos duplicados; no arrancar
otro driver para consultar su estado.

Para instalar o actualizar las dependencias, compilar y ejecutar las pruebas:

```bash
bash install.sh
```

El instalador muestra una barra de ocho etapas completadas, de 0 a 100%.
El porcentaje no estima el tiempo restante: compilación y pruebas pueden tardar
más que otros pasos. Los mensajes de los comandos permanecen visibles y, ante
un error, se detiene sin marcar la etapa como completada.

Ejecutar como usuario normal desde una instalación Debian/Raspberry Pi OS.
El instalador solicita `sudo` para `apt-get update` y la instalación de paquetes;
compila con dos trabajos y ejecuta `make test` como el usuario actual. No hace
una actualización general del sistema, no descarga cambios Git y no inicia
el driver. Conserva `configdb.cfg` y no modifica bases de datos ni servicios.
Las pruebas de correo y Modbus utilizan servidores locales simulados.

Si esta máquina alojará la base de datos, instalar por separado:

```bash
sudo apt-get install -y mariadb-server mariadb-client
```

Los scripts `autoiniciar.sh`, `mkautoiniciar.sh` y `mkautoiniciar.bash` se
retiraron de la rama de pruebas. Si un servicio, cron o arranque de escritorio
los referencia, revisar y migrar esa configuración antes de actualizar la
instalación activa. El nuevo instalador no configura arranque automático.

En la rama mejorada, SIGINT/SIGTERM solicitan parada ordenada. Las operaciones
de red en curso pueden retrasarla, especialmente SMTP, que conserva los tiempos
predeterminados de libcurl sin un límite total añadido. No se garantiza la
persistencia de muestras que todavía estén pendientes en memoria.

## Monitor SSH: automonitorctl

Con el driver mejorado ya iniciado, abrir otra terminal en la raíz del proyecto:

```bash
./bin/automonitorctl
./bin/automonitorctl --once
./bin/automonitorctl --json
./bin/automonitorctl --device NOMBRE_DEL_EQUIPO
```

El visor consulta un socket local: no inicia adquisición ni hace consultas
Modbus adicionales. `--socket RUTA` permite indicar otro socket. Las flechas
seleccionan dispositivos/tags, Enter abre el detalle, Esc vuelve, R actualiza
y Q cierra únicamente el visor. Muestra estados, valores, períodos, antigüedad,
errores, alarmas y eventos.

### Qué significa «Lecturas tags/s»

Es el incremento del contador de llamadas completadas a lectura de tags dividido
por los segundos entre instantáneas del monitor. Incluye llamadas exitosas y
fallidas; excluye las omitidas por suspensión/espera de reconexión. No mide exclusivamente
datos válidos ni peticiones físicas Modbus.

- Un tag es una variable; leer el mismo valor otra vez también cuenta.
- Los reintentos internos no cuentan como llamadas independientes del tag.
- Un bloque Modbus puede abastecer varios tags y contabilizar varias lecturas.
- Las omisiones por suspensión o recuperación se contabilizan aparte en P.
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
./bin/automonitor-mailcheck --config /ruta/configdb.cfg
./bin/automonitor-mailcheck --config /ruta/configdb.cfg --ports 465,587
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

## Versión y trazabilidad

La fuente única de versión de producto es `src/include/version.h`.
`make` incluye el commit y la fecha UTC de compilación en el driver. Una
compilación directa sin esas definiciones muestra `unknown` para esos metadatos.

```bash
./bin/automonitorv1 --version
./bin/automonitorctl --version
```

El visor muestra la versión del driver conectado; `--version` identifica al
visor instalado. El JSON conserva `version: 1` como versión del protocolo y
expone por separado `driver_version`, `driver_commit` y `build_date`.
Para distribuir el visor fuera de `bin`, copiar también su `version.h` vecino.
La etiqueta `v1.2.0` identifica la entrega en la rama mejorada; no implica integrar
la rama en `main`. Consultar el historial para funciones y limitaciones.

## Distribución sin fuentes

Después de `make test`, ejecutar `make dist`. Se crea una carpeta nueva bajo
`dist/` con `bin/` (driver, visor, diagnóstico y metadatos de versión), `html/`
y `LEEME.txt`. No contiene fuentes C++, tests, objetos, Git ni configuración
privada. Las herramientas Python y `version.h` son dependencias de ejecución.
La distribucion no incluye un supervisor ni configura arranque automatico.

Copiar el `configdb.cfg` autorizado a la raíz de esa distribución y protegerlo
con `chmod 600`. Ejecutar el driver desde su `bin/`. Usar la distribución en
una máquina con arquitectura y bibliotecas compatibles: no incluye las
bibliotecas compartidas. No publicar esa carpeta después de añadir credenciales.

`bash install.sh` realiza compilación, pruebas, distribución y limpieza de
objetos. `make clean-objects` elimina solo objetos/dependencias intermedios y
conserva `bin/`; `make clean` elimina además los ejecutables ordinarios. Los
fuentes y pruebas se conservan en el checkout para mantenimiento y actualización.
La actualización de esta rama retira los binarios históricos versionados de
`bin/`; compilar antes de volver a arrancar y adaptar los servicios que usen
la ruta anterior `.build`.

## Instalación y actualización recomendadas

Desde el repositorio, como usuario normal: `bash install.sh --test-s7-sql`.
Instala dependencias, compila, ejecuta regresión y SQL aislado, genera distribución
y limpia objetos, conservando configdb.cfg. Para omitir SQL aislado usar
`bash install.sh`. No ejecutar después make manualmente ni copiar la biblioteca
Snap7 del cloud a la Raspberry: el script la compila para la máquina destino.
