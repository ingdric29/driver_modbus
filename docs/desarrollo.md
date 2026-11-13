# Desarrollo

Dependencias Debian: `build-essential`, `libmodbus-dev`, `libmysqlcppconn-dev`,
`libcurl4-openssl-dev`, `libboost-filesystem-dev`, `libboost-system-dev`.

```bash
make -j2
make test
```

Los ejecutables nuevos son `.build/automonitorv1` y `.build/automonitorctl`
([monitor SSH](monitor.md), Python 3/curses). Objetos y dependencias quedan en
`.build`, sin sobrescribir los ejecutables/objetos históricos versionados.
Los cambios de headers se detectan automáticamente. `make orquesta` construye
el supervisor opcional en `.build/orquesta`; las pruebas no lo ejecutan.

Para ejecutar con una configuración autorizada, usar `.build` como directorio
de trabajo: el programa busca configuración, plantillas y logs en su padre.
Los scripts de autoinicio históricos apuntan a `bin`; su despliegue debe
ajustarse al binario nuevo antes de utilizarlos. No se han ejecutado ni se han
reiniciado servicios del usuario durante estas modificaciones.

En el entorno cloud se usó `/workspace/driver-env/build.sh`, con bibliotecas
Debian verificadas extraídas localmente. El makefile admite `INCLUDES`, `LIBS`,
`CPPFLAGS`, `LDFLAGS`, `CXX`, `CXXFLAGS`, `BUILD_DIR` y `TARGET` para ese caso.
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
