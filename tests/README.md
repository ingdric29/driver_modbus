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
