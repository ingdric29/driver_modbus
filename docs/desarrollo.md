# Desarrollo

El código propio usa C++11, cuatro espacios, finales de línea LF y el estilo
de `.clang-format` (clang-format 19). No reformatear automáticamente las
bibliotecas incorporadas INIReader/ini/rs232/Snap7 ni los fixtures históricos de pruebas.
Los mensajes operativos de consola usan español y las etiquetas `[ OK ]`,
`[ ESTADO ]`, `[ AVISO ]` y `[ ERROR ]`. Los identificadores técnicos, campos de
métricas y diagnósticos de las bibliotecas conservan sus nombres originales.

Las tres etapas de [modularización](propuesta_modularizacion.md) están
implementadas: repositorios/catálogos, trabajadores y equipos/conversiones/alarmas.
Las clases y firmas públicas se conservan, así como la ubicación y orden de
inicialización de globales y estáticos; los runners enlazan los módulos nuevos.

Dependencias Debian: `build-essential`, `libmodbus-dev`, `libmysqlcppconn-dev`,
`libcurl4-openssl-dev`, `libboost-filesystem-dev`, `libboost-system-dev`.
Snap7 se compila desde las fuentes fijadas en `third_party/snap7`; no necesita
descargas ni una instalación global. Véase [integración S7](siemens_s7.md).

```bash
bash install.sh
```

Los ejecutables nuevos son `bin/automonitorv1` y `bin/automonitorctl`
([monitor SSH](monitor.md), Python 3/curses). Objetos y dependencias quedan en
`.build`; los ejecutables generados de `bin/` no se versionan.
Los cambios de cabeceras se detectan automáticamente. Orquesta fue retirado;
el driver se inicia directamente o mediante un servicio configurado aparte.

Para ejecutar con una configuración autorizada, usar `bin` como directorio
de trabajo: el programa busca configuración, plantillas y logs en su padre.
El instalador Linux se ejecuta con `bash install.sh` como usuario normal:
actualiza el índice APT, instala dependencias, compila y ejecuta las pruebas.
También crea `dist/` sin fuentes del driver ni configuración privada (incluye las
fuentes correspondientes de Snap7 y sus licencias) y limpia los objetos
intermedios. No inicia servicios ni modifica configuración. Los scripts históricos de
autoinicio se retiraron; migrar cualquier referencia externa antes de actualizar
una instalación que los utilice.

Para bibliotecas fuera del sistema, el makefile admite `INCLUDES`, `LIBS`,
`CPPFLAGS`, `LDFLAGS`, `CXX`, `CXXFLAGS`, `BUILD_DIR`, `BIN_DIR` y `TARGET`.
Las rutas de dependencias son propias de cada entorno y no se incluyen en Git.
Los repositorios cloud ya están aislados; trabajar sobre el checkout existente
sin crear worktrees salvo petición explícita.

`make test` ejecuta once programas unitarios con ASan/UBSan y un runner enlazado
con las fuentes reales. Incluye servidores Modbus TCP y RTU sobre loopback y
pseudoterminales, parsing RS232, periodos distintos y parada por señal.
También verifica el socket de monitorización, clientes lentos y el visor Python
en una pseudoterminal (navegación, redimensionado y restauración de terminal).
Para compiladores sin sanitizadores, establecer `TEST_CXXFLAGS` explícitamente.

`make test-sql` es opcional y requiere la instancia de prueba descrita en
[tests/README.md](../tests/README.md). No es el esquema de producción. Los
programas usan perfiles locales reservados de pruebas y no consultan las
credenciales de configdb.cfg. La prueba de comandos crea `driver_cmd_test`.
La prueba de arranque utiliza Python 3 y confirma que la ausencia o el formato
de las antiguas claves de licencia no impiden alcanzar el coordinador.

No se han validado equipos industriales físicos, correo,
SMS ni una ejecución completa sobre el esquema del usuario. El envío SMS
heredado sigue desactivado; ahora devuelve error determinista en vez de usar
un resultado sin inicializar. La compilación conserva advertencias heredadas
de variables sin uso y conversiones de signo; no son pruebas de funcionalidad.

## Comentarios y contratos

Los archivos propios incluyen un encabezado con descripción, datos necesarios
y retornos. Las funciones de implementación documentan parámetros, resultado
y efectos importantes, incluidos bloqueos, SQL, envíos y operaciones destructivas.
No asumir un único código de éxito: lecturas, catálogos, trabajadores y CURL
usan convenciones distintas. Los comentarios describen el comportamiento actual,
incluidas sus limitaciones; no prometen validaciones o confirmaciones inexistentes.

Al cambiar una firma o comportamiento, actualizar su contrato y el encabezado.
Mantener comentarios concisos que expliquen el propósito, sin repetir cada
instrucción. Los encabezados propios no incluyen nombres personales. Los avisos
de licencia/atribución de INIReader, ini y RS232 y los fixtures históricos se
conservan sin modificaciones.

## Verificación al cierre — 2026-11-16

Las tres etapas se validaron en Linux con make test y el perfil reservado de
make test-sql. Se comprobaron también versiones y distribución sin fuentes.
Los cambios de comentarios conservaron los tokens ejecutables; la barra del
instalador se ensayó con comandos simulados, incluyendo errores por etapa.
No confundir esas comprobaciones con instalación real de paquetes o validación
de Windows/hardware industrial. Evidencia y commits en el
[registro de jornada](registro_2026-11-16.md).

Las tres etapas de separación no implementan automáticamente los cambios
funcionales pendientes. No mezclar futuras modificaciones de precisión,
retención o comandos con una nueva reorganización sin pruebas específicas.

## Vía recomendada de instalación y actualización

Usar `bash install.sh` en Debian/Raspberry como usuario normal. El instalador
comprueba/instala dependencias, compila Snap7 y el driver, ejecuta la regresión,
crea distribución con licencias/fuentes Snap7 y limpia objetos; conserva el CFG.
Para incluir la prueba SQL mixta aislada: `bash install.sh --test-s7-sql`.
Esta opción prepara además MariaDB Server/Client como dependencias de prueba.
El test crea un servidor temporal separado, no usa dbdevlog ni sus credenciales.
No inicia adquisición al terminar; arrancar desde bin con ./automonitorv1.

`--skip-packages` permite usar el mismo flujo con dependencias ya preparadas,
sin sudo/APT (por ejemplo cloud). No verifica ni instala paquetes de sistema;
la compilación y pruebas detectan dependencias faltantes. Admite INCLUDES y LIBS
en el entorno para las bibliotecas locales, y S7_TEST_SYSROOT para el fixture SQL.
Las instrucciones make de informes anteriores describen pasos internos; no es
necesario repetirlos manualmente tras un instalador exitoso.
