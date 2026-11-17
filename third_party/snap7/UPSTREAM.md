# Snap7 vendorizado

- Proyecto: https://github.com/davenardella/snap7
- Commit fijo: `30f37da3114024a71ba93f7fd855c680b97a406f`.
- Versión del núcleo: 1.4.3. El wrapper C++ conserva su cabecera histórica 1.4.0.
- Procedencia: `automonitor-s7.zip`, suministrado por el usuario. Los 35 archivos seleccionados se compararon byte a byte con ese commit oficial antes de incorporarlos.
- Selección: fuentes `src/`, wrapper C/C++ y textos GPL/LGPL. Sin `.git`, binarios, objetos ni ejecutables de ensayo del adjunto.
- Integridad: `SHA256SUMS`, comprobado por `tests/test_snap7_sources.py`.
- Cambios locales en fuentes upstream: ninguno. Las reglas de compilación pertenecen al makefile del driver.

Los encabezados declaran LGPL versión 3 o posterior. Se conservan los textos `lgpl-3.0.txt` y `gpl.txt`. La biblioteca se enlaza dinámicamente y se distribuye reemplazable como `bin/lib/libsnap7.so`. El paquete incluye los avisos y un archivo con estas fuentes exactas, el wrapper y las instrucciones de compilación, sin incluir fuentes del driver ni configuración privada.

Compilación de biblioteca desde este directorio (Linux con GCC; usar la arquitectura de destino):

```bash
mkdir -p build
g++ -std=c++11 -O2 -fPIC -pthread -Isrc/sys -Isrc/core -Isrc/lib \
  -shared -Wl,-soname,libsnap7.so src/sys/*.cpp src/core/*.cpp src/lib/*.cpp \
  -pthread -lrt -o build/libsnap7.so
```

Para reconstruir también el wrapper de la aplicación, usar `release/wrappers/c-cpp/snap7.cpp` con su cabecera. El repositorio del driver contiene las reglas de enlace. No modificar las fuentes upstream para adaptar rutas o herramientas. En esta etapa no se habilita adquisición S7 ni se contacta con PLC.
