# Desarrollo

El código propio usa C++11, cuatro espacios, finales de línea LF y el estilo
de `.clang-format` (clang-format 19). No reformatear automáticamente las
bibliotecas incorporadas INIReader/ini/rs232 ni los fixtures históricos de pruebas.
Los mensajes operativos de consola usan español y las etiquetas `[ OK ]`,
`[ ESTADO ]`, `[ AVISO ]` y `[ ERROR ]`. Los identificadores técnicos, campos de
métricas y diagnósticos de las bibliotecas conservan sus nombres originales.

Las tres etapas de [modularización](propuesta_modularizacion.md) están
implementadas: repositorios/catálogos, trabajadores y equipos/conversiones/alarmas.
Las clases y firmas públicas se conservan, así como la ubicación y orden de
inicialización de globales y estáticos; los runners enlazan los módulos nuevos.

Dependencias Debian: `build-essential`, `libmodbus-dev`, `libmysqlcppconn-dev`,
`libcurl4-openssl-dev`, `libboost-filesystem-dev`, `libboost-system-dev`.

```bash
make -j2
make test
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
También crea `dist/` sin fuentes ni configuración privada y limpia los objetos
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
