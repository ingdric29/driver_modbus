# Integración Siemens S7: dependencia y compilación

Estado actual: adquisición de DB S7 y Modbus con monitor y persistencia SQL.
El modo SQL integra también alarmas y eventos Siemens. La lectura del PLC Siemens
sigue siendo de solo lectura; el modo local INI no carga reglas SQL.
El alcance completo está en
[acuerdos](acuerdos_siemens_s7.md).

## Dependencia reproducible

Snap7 1.4.3 se fija al commit `30f37da3114024a71ba93f7fd855c680b97a406f`
de https://github.com/davenardella/snap7. Los fuentes y wrapper del ZIP adjunto
se verificaron byte a byte contra ese commit. No se usan sus binarios ARM ni
su historial Git. Los 35 archivos upstream se conservan sin modificaciones,
incluidos encabezados y licencias; `SHA256SUMS` permite comprobarlos sin red.

El wrapper conserva la identificación histórica 1.4.0 de su cabecera; no es
la versión del núcleo. La declaración de licencia es LGPL-3.0-or-later;
se incluyen también los términos GPL que incorpora esa licencia.

## Compilación y pruebas

Con las dependencias normales del driver instaladas:

```bash
make -j2
make test-snap7
make test
make dist
```

`make` compila una biblioteca compartida para la arquitectura actual y enlaza
el wrapper C++ al ejecutable. Salidas: `bin/automonitorv1`, `bin/lib/libsnap7.so`
y objetos bajo `.build/third_party/snap7/`. El enlace usa `$ORIGIN/lib` para
resolver Snap7 junto al ejecutable, sin instalarla globalmente ni depender del
directorio de trabajo. Se conservan los overrides de dependencias existentes;
la receta cloud concreta está en el [informe de etapa 0](etapas/00_linea_base.md).

`make test-snap7` comprueba:

- Hashes y conjunto exacto de fuentes upstream.
- Creación/destrucción de clientes, aislamiento de parámetros y fallo de lectura
  sin conexión. No establece conexiones ni escribe a PLC.
- Paquete real trasladado a otra ubicación, resolución de la biblioteca,
  ejecución de `--version`, presencia de licencias/fuentes y ausencia de
  configuración privada y fuentes de aplicación. Una biblioteca corrupta
  provoca un fallo real del cargador.

`make test` incluye estas verificaciones y la regresión existente sin SQL.
`make clean` elimina también los objetos/biblioteca/prueba de Snap7 generados.
No se deben ejecutar formateadores sobre `third_party/snap7`.

## Configuración y adquisición local (sin SQL)

Copiar `examples/local-acquisition.ini` a una ubicación local y ajustar destinos
y mapas autorizados. El ejemplo usa loopback, no la IP de un equipo productivo.

```bash
cd bin
./automonitorv1 --check-local-config /ruta/local.ini
./automonitorv1 --local-config /ruta/local.ini
# En otra terminal, desde la raíz de esa instalación:
./bin/automonitorctl --once
```

No se lee configdb.cfg en este modo y no hay conexiones ni persistencia SQL.
El arranque sin estas opciones conserva el flujo normal del driver. Un único
driver posee el socket y lock de `.runtime` de la instalación; el monitor
consulta ese mismo proceso, sin iniciar otra adquisición.

INIReader existente carga secciones `device:nombre` y `tag:dispositivo:nombre`.
Los nombres admiten minúsculas ASCII, números, `_` y `-`, hasta 30 caracteres.
Campos de dispositivo: protocol (SIEMENS_S7 o MBTCP), host IPv4 numérica, port,
rack, slot, timeout_ms, slave y enabled (0/1). Defaults: S7 puerto 102, rack 0,
slot 1, timeout 1000 ms; MBTCP puerto 502/esclavo 1. Campos de tag: address,
type, period (segundos, mínimo 0.02), scale multiplicativo y unit. Para MBTCP
local: types word/word_signed y function hr/rr; la configuración SQL normal
conserva todos sus tipos y protocolos anteriores.

Se valida el catálogo completo antes de conectar: campos desconocidos, destinos
duplicados, referencias ausentes y tipos/rangos inválidos provocan error explícito.
Límites: 256 KiB por archivo, 64 dispositivos y 2048 tags. Los deshabilitados
no conectan. Un trabajador por PLC S7 aísla bloqueos y fallos; Modbus mantiene
un trabajador de protocolo para conservar sus buffers estáticos heredados.
No se cambia la precisión histórica de Modbus; S7 decodifica y publica double
sin pasar por campos float heredados. Fallos conservan el último valor válido
con calidad ERROR/REINTENTANDO, nunca como una muestra nueva.

`make test-local` ejecuta el driver real con dos servidores Snap7 y uno Modbus
en loopback, verifica los ocho tipos, escalamiento, aislamiento ante caída,
recuperación, recarga y apagado. Se incluye en `make test`.

## Agrupación, diagnóstico y recarga

Se agrupan exclusivamente tags vencidos contiguos/superpuestos del mismo PLC y
DB. El límite útil deriva de la PDU negociada menos los 18 bytes del encabezado
de respuesta de lectura de bytes; antes de conectar se usa un límite conservador
de 222 bytes. Snap7 conserva además su fragmentación interna. No se adelanta un
tag lento por compartir bloque con uno rápido. Cada muestra lleva la hora de
finalización de su solicitud; las derivadas del mismo bloque comparten reloj.
Si el PLC rechaza un rango agrupado, se intenta cada tag por separado para no
invalidar vecinos correctos. Un fallo TCP/ISO no dispara ese fallback.

`automonitorctl --once --device plc` muestra dirección y último diagnóstico.
`--json` incluye `s7_links` con PDU, intentos, conexiones, fallos y llamadas
DBRead por dispositivo/generación. Son llamadas lógicas de biblioteca, no un
conteo de paquetes TCP. Las métricas generales acumulan resultados por tag e
intentos/reutilizaciones de conexión; no se reinician al recargar. Los eventos
son acotados y los errores idénticos no generan una línea por ciclo.

Para recargar, reemplazar el INI completo de forma atómica y enviar SIGHUP al
PID mostrado por el monitor (por ejemplo, `kill -HUP PID`). El monitor es de
solo lectura y no envía órdenes. Se valida el candidato mientras los trabajadores
actuales siguen funcionando; si es inválido se conserva la generación vigente.
Si es válido se detienen/unen todos los trabajadores antes de reemplazar el
catálogo y reabrir sesiones. Se pueden añadir, modificar o deshabilitar equipos.
Los valores y contadores por tag reinician con la generación; el socket y PID
permanecen. SIGTERM/Ctrl+C interrumpen esperas y cierran sesiones tras la operación
actual, acotada por los timeouts; no hay cola SQL que vaciar en este modo.

La [guía física por SSH](validacion_raspberry_s7.md) detalla los pasos pendientes
para cerrar la etapa 6. Los resultados físicos anteriores del usuario no se
presentan como prueba de esta integración nueva.

## Distribución

El paquete contiene `bin/lib/libsnap7.so`, avisos en `licenses/snap7/` y fuentes
correspondientes con receta en `share/snap7-source.tar.gz`. La biblioteca puede
reemplazarse por una reconstrucción compatible de esa misma arquitectura/ABI.
Las demás bibliotecas del sistema siguen siendo requisitos externos.

Se validó Linux x86_64. La compilación y adquisición integrada en Raspberry Pi
5 ARM64 con el S7-1200 están pendientes de la etapa 6; las pruebas físicas
previas del usuario corresponden a los ensayos independientes adjuntos.

## Persistencia SQL en el arranque habitual

La integración SQL ya está disponible; reemplaza la limitación histórica de
etapas 0–5. Arrancar sin `--local-config` usa configdb.cfg y carga también S7
habilitados del concentrador configurado. Se requieren `tbdispositivos.rack`,
`slot` y `tbregistros.direccion_siemens`; tipos exclusivamente `s7_*`.
No se modifica ni depende de agregar esas columnas a qrytablacom.

El [informe SQL](etapas/07_sql_siemens.md) describe rastro, instantáneos,
histórico, transacciones, reintentos y límites de RAM. `make test-s7-sql`
es opt-in y usa un servidor temporal aislado, sin esquema del usuario.
La persistencia física en Raspberry debe comprobarse antes de declararla validada.

## Representación decimal de REAL

REAL se convierte al decimal corto que recupera el mismo float Siemens, antes
 de aplicar el escalamiento: con escala 1, 0x41CCCCCD se publica y almacena como
25.6, sin expandirlo a 25.600000381469727. El monitor y SQL comparten el formato.
No es un redondeo fijo de todos los tipos ni una alteración de Modbus. Véase el
[informe de precisión](etapas/08_real_decimales.md), incluidos límites y pruebas.

## Instalación con el script existente

Para instalar/actualizar y verificar S7 junto con Modbus usar
`bash install.sh --test-s7-sql`. `bash install.sh` conserva la suite ordinaria
sin iniciar SQL temporal. Ambos compilan Snap7, generan distribución y conservan
configdb.cfg. En ADQUISICION el arranque normal muestra ahora Tags Siemens S7 y
PLC Siemens S7 con los conteos del catálogo filtrado por concentrador, incluso
cuando son cero. No cuenta las lecturas ni las filas históricas.

## Alarmas y eventos Siemens (modo SQL)

El arranque habitual también publica las muestras S7 en memoria para los
mismos evaluadores de alarmas y eventos del driver. No se agregan lecturas PLC
ni consultas de valores SQL por evaluación. Solo se incluyen dispositivos
habilitados y tags del concentrador configurado.

En `tbdefalarmas`, usar el nombre de `tbdispositivos` y el tag de `tbregistros`:

```text
Dispositivo: Siemens_S7_1200
Tag / valorAlarma: test_real
Activar:    $(Siemens_S7_1200.test_real)>30
Desactivar: $(Siemens_S7_1200.test_real)<29
Acciones:   EMAIL[7];
```

Los nombres son exactos y las expresiones mantienen la sintaxis existente:
`=` igualdad, `~` desigualdad, `&` AND, `|` OR, `@` AND de bits. Se pueden
combinar referencias Siemens y Modbus, por ejemplo:

```text
$(Siemens_S7_1200.test_bool)=1 & $(PLC_TM172.iCfgSpHR)>50
```

Se usa el valor con escalamiento, en double y sin pasar por float/int heredados.
El resultado alimenta `tbhistoalarmas`, las acciones y el estado del monitor.
EMAIL reutiliza el directorio, plantilla, credenciales y envío actuales. CMD
puede actuar sobre los destinos Modbus ya soportados; esto no implementa escritura
Siemens. Un CMD ausente/inválido no debe impedir intentar EMAIL, según las reglas
actuales. No hay cola persistente nueva ni cambios de política SMTP.

Un fallo, espera de reconexión, muestra ausente/no finita o muestra con edad
superior a `max(1 segundo, 3 × período del tag)` impide evaluar esa referencia.
Se conserva el estado previo de la alarma y se muestra sin evaluar; no se
interpreta como cero ni desactivación. Una alarma independiente de Modbus sigue
su evaluación. Las expresiones con varios tags toman snapshots por referencia,
no una captura atómica entre PLC distintos.

En `tbdefeventos` se reutilizan `nombredispositivo`, `tag`, `bit1`, `activado` y
`desactivado`. `tbhistoeventos` conserva sus ocho campos habituales:

- `bit1=-1`: cambio de valor completo, incluido REAL y DWORD sin truncar a 16 bits.
- BOOL: se recomienda `bit1=-1`; también admite bit 0 del booleano ya decodificado.
  Un tag DBX0.7 ya representa ese bit: no se configura otra vez bit1=7.
- BYTE/SINT: bits 0..7; WORD/INT: 0..15; DWORD/DINT: 0..31. Los bits se extraen
  del entero original antes del escalamiento; los negativos usan complemento a dos.
- REAL requiere `bit1=-1`. Un selector incompatible deja ese evento sin referencia
  y registra un aviso al cargar el catálogo; no detiene los demás.
- La primera muestra válida fija la referencia sin generar transición. Después,
  BOOL/bits usan `activado` para 1 y `desactivado` para 0. Para cambios completos
  numéricos se conserva la convención del camino anterior: texto `desactivado`
  seguido del nuevo valor. Conviene usar ahí un texto como «Valor actualizado».

Las evaluaciones usan los períodos existentes: `periodo_revision_alarmas`
(predeterminado 60 s) y `GLOBAL_periodo_revision_eventos` (4 s). No cambia el
muestreo de tags ni la resolución/planificación heredada de sus revisiones.
Se comparan últimas muestras: pueden perderse cambios que
ocurran entre lecturas o revisiones; no es un registro de todas las transiciones
internas del PLC. Para pulsos breves hace falta retención/contador en el PLC.

No se modifica la estructura SQL ni la página. Sí deben existir las definiciones
que se quieran habilitar en tbdefalarmas/tbdefeventos y los destinatarios. Reiniciar
tras configurar o usar la recarga del catálogo existente. No se crea ninguna
alarma en producción automáticamente. Tras reinicio/recarga se mantienen las
reglas de inicialización anteriores de alarmas; no hay deduplicación persistente.

`make test-s7-sql` incluye un perfil con Snap7, Modbus, MariaDB y SMTP temporales:
transiciones, bits altos, precisión, acciones, caída/recuperación y eliminación
del dispositivo durante recarga. [Informe](etapas/18_alarmas_eventos_siemens.md).
