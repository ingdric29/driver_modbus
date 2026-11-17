# Etapa 2: direcciones DB y tipos Siemens

Fecha: 17 de noviembre de 2026. Rama: automonitor-modbus-driver-V1.2.0-pruebas.

Se incorpora `s7_decode.h`, independiente de Snap7 y SQL: parser estricto de direcciones DBX/DBB/DBW/DBD, tipos BOOL/BYTE/SINT/WORD/INT/DWORD/DINT/REAL, validación de compatibilidad y decodificación big-endian. Acepta tipos en mayúsculas y los nombres `s7_*` del diseño. No modifica las conversiones Modbus.

DB permitidos: 1..65535. El rango de bytes respeta el direccionamiento de bits S7ANY de 24 bits; se comprueba el extremo completo de cada valor. Bits 0..7. No se impone alineación par que S7 no requiere. Se rechazan signos, espacios, sufijos, números excesivos, buffers nulos/cortos, tipos incompatibles y resultados no finitos, incluido desbordamiento del escalamiento. Los tipos con signo se convierten sin desplazamientos con signo ni desbordamiento.

## Verificación

- `g++ -std=c++11 -Wall -Wextra -Werror -fsanitize=address,undefined -Isrc/include tests/test_s7_decode.cpp -o /tmp/test_s7_decode && /tmp/test_s7_decode`: aprobado.
- `make -j2 test` con los overrides locales documentados en etapa 0: código 0; log `/workspace/driver-modbus-env/stage2-test.log`.
- El runner unitario incorpora `s7_decode`; casos de todos los tipos, bits 0..7, enteros mínimos/máximos, REAL positivo/negativo, NaN/Inf, escalas negativas/cero y errores de dirección/tamaño.
- Regresión existente, integridad Snap7, ABI y distribución: aprobadas. No se ejecutaron pruebas SQL ni se contactó con PLC.

Durante el desarrollo las pruebas detectaron y permitieron corregir el límite de un dígito y la promoción de enteros con signo antes del cierre. No quedan fallos requeridos pendientes. La validación de tipos reales del equipo físico permanece para etapa 6.

Etapa 2 validada; SHA y comprobación remota se comunican al crear el commit. Siguiente: transporte de solo lectura.
