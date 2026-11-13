# Pruebas sin hardware

Desde la raíz del repositorio, con GCC y C++11:

```bash
build_dir=$(mktemp -d /tmp/driver-tests.XXXXXX)
g++ -std=c++11 -Wall -Wextra -Werror -fsanitize=address,undefined \
  -Isrc/include tests/test_modbus_decode.cpp -o "$build_dir/decode"
"$build_dir/decode"
g++ -std=c++11 -fsanitize=address,undefined -Isrc/include \
  tests/test_numeric_format.cpp src/conversiones.cpp -o "$build_dir/format"
"$build_dir/format"
```

Se comprueban el orden de palabras de 32/64 bits, valores máximos,
conversiones a entero fuera de rango y formatos numéricos grandes.
Estas pruebas no requieren licencia, base de datos ni dispositivos.
No validan todavía concurrencia, adquisición física ni persistencia.

## Ciclo de vida y concurrencia

`test_workers.cpp` comprueba diez generaciones de cuatro trabajadores,
incluyendo excepciones durante el arranque. Compilar con C++11, `-pthread`
y `-Isrc/include`; admite ASan/UBSan.

`bash tests/run_state_test.sh` enlaza el driver real sin ejecutar su `main`:
valida parsing RS232, invalidez de una lectura vacía y mil publicaciones
concurrentes con lectura coherente de valor entero/flotante. Requiere las
bibliotecas de desarrollo del driver, pero no SQL, licencia ni hardware.
Acepta `CPPFLAGS` y `LDFLAGS` para bibliotecas instaladas fuera del sistema.

`test_periods.cpp` utiliza tiempo simulado: verifica 101 lecturas a 100 ms,
6 a 2 s y 3 escrituras a 5 s durante diez segundos, además de saltos de
intervalos vencidos y rechazo de periodos inválidos. Compilar como las
pruebas de decodificación, con `-Isrc/include` y ASan/UBSan.

`test_sample_window.cpp` comprueba un pico de voltaje seguido de recuperación,
la fecha de adquisición del extremo, mínimo/último valor, modo desactivado,
deduplicación y rechazo de NaN. Compilar con C++11 y `-Isrc/include`.

`test_pending_batch.cpp` inyecta un fallo en la segunda de tres muestras:
comprueba que el reintento guarda solo el sufijo pendiente y que un lote
confirmado no se vuelve a enviar. No requiere SQL.

`test_modbus_transport.cpp` requiere libmodbus. Ejecuta servidores de prueba
TCP en un puerto loopback libre y RTU mediante pseudoterminales Linux.
En ambos casos realiza lectura/escritura/lectura en la conexión retenida,
comprueba los valores y cierra los recursos. Añadir `-pthread`, los includes
y `-lmodbus` a la compilación. Usar un timeout externo de 10 segundos.

`test_register_batch.cpp` verifica combinación de rangos contiguos/solapados,
separación por huecos y límite de 20 palabras, rangos inválidos y descarte de
caché al terminar el ciclo. El runner de estado también comprueba con el
driver real que dos tags contiguos usan una consulta inicial de dos registros
y que la siguiente consulta del tag rápido pide uno solo, sin adelantar el lento.

## SQL opcional

`test_sql_session.cpp` utiliza exclusivamente una instancia temporal local en
`127.0.0.1:33077`, usuario root sin clave y base `driver_test`. Ese perfil es
solo de pruebas; no se debe usar con servidores de producción. Crea y elimina
`history_fixture`; comprueba reutilización, parámetros largos/con comillas,
rollback de un lote parcial y rechazo de identificadores inválidos.

Con esa misma instancia, `DRIVER_SQL_TEST=1 bash tests/run_state_test.sh`
comprueba los wrappers reales de inserción/actualización y propagación de
fallos mediante `generic_fixture`. Las pruebas ordinarias no requieren SQL.

`test_alarm_expression.cpp` cubre precedencia, negación, números negativos y
exponentes, y rechaza operandos ausentes, paréntesis inválidos y tokens extra.

El runner de estado verifica búsqueda de eventos RTU/RS232 y referencias
inexistentes. Con `DRIVER_SQL_TEST=1` requiere además `driver_test_replica`:
replica fechas iguales en lotes 2/2/1, copia datos tardíos y comprueba rollback,
checkpoint sin avance y recuperación posterior, usando `replication_fixture`.

`test_retry.cpp` verifica crecimiento 100/200 ms, límite de 30 s y reinicio de
la espera tras recuperación, usando tiempo simulado. La prueba enlazada de
estado incluye SIGTERM: interrumpe una espera de 60 s en menos de un segundo,
sin alterar las pruebas posteriores.

## Comandos agrupados

Desde la raíz, `make test` ejecuta las pruebas ordinarias y `make test-sql`
ejecuta la integración opt-in con la instancia local dedicada. Los runners
crean sus ejecutables y logs en carpetas temporales, con timeout y limpieza.
La suite de formato valida también fechas inválidas y roundtrip de fechas.
WorkerGroup comprueba además propagación de excepciones después de unir hilos.

La integración de comandos crea `driver_cmd_test`, con tablas temporales de
perfil, y verifica concentradores con comillas/porcentajes, carga de comandos,
consumo de su orden y marca de confirmación. No ejecuta esos comandos contra
dispositivos. El caso SMS solo verifica el error del emisor desactivado.

`make test` también ejecuta `test_startup_without_license.py` con Python 3 y
el binario construido. Usa configuración temporal y un puerto loopback sin
servicio para verificar tres arranques: sin claves antiguas, con cero y con
texto inválido. Los tres deben alcanzar el coordinador y devolver el fallo SQL
esperado; no se afirma adquisición funcional con la base de datos ausente.

## Regresión de EMAIL[7] y arranque completo

La prueba enlazada utiliza `fake_email.cpp` en lugar del transporte SMTP real:
verifica una alarma creada antes del directorio, activación/desactivación,
reemplazo del directorio, ID inexistente y directorio nulo, sin enviar correo.
`TEST_CXXFLAGS='-fsanitize=address,undefined -fno-omit-frame-pointer'` también
instrumenta esa prueba enlazada.

`make test-sql` añade `test_full_startup.py`: prepara el perfil local reservado
`driver_startup_fixture` con 159 tags TCP, un servidor Modbus loopback y una
alarma `EMAIL[7]`. Comprueba que alcanza la acción, mantiene un histórico con
clave foránea válida y termina por SIGTERM sin caída ni error de integridad.
Usa una plantilla vacía para que el transporte real no abra SMTP. Solo debe
usarse con la instancia local dedicada de pruebas en el puerto 33077.
Requiere el cliente MariaDB/MySQL; `MARIADB_CLIENT` permite indicar su ruta.

## Monitor SSH

`make test` incluye `test_monitor.cpp` con ASan/UBSan: períodos distintos,
calidad y edad con reloj simulado, recuperación, aislamiento de generaciones,
SQL observado, eventos acotados, JSON/UTF-8, permisos, socket abandonado,
rechazo de segundo servidor y espectadores lentos durante publicaciones concurrentes.
`python3 tests/test_monitor_client.py` usa un socket simulado y una PTY real:
verifica consultas de solo lectura, selección, navegación a tags, redimensionado,
restauración de terminal y que Q deja la fuente disponible. No inicia un driver.

Con MariaDB local de pruebas, `make test-sql` ejecuta el proceso completo con
159 tags TCP simulados, consulta el monitor y el CLI, conecta varios espectadores,
recarga el catálogo mediante tbcambios y verifica el cierre ordenado del socket.
La plantilla de correo vacía evita cualquier envío SMTP.
