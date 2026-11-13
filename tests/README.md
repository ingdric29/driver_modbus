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
