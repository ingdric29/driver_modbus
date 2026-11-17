# Etapa 1: Snap7 en compilación y distribución

Fecha: 17 de noviembre de 2026. Rama: `automonitor-modbus-driver-V1.2.0-pruebas`.
Base: etapa 0, commit `ed0e573aba568a8327f53d1234c732791e18e873`, publicado y verificado en GitHub antes de iniciar estos cambios.

## Objetivo y cambios

Incorporar una dependencia Snap7 reproducible y comprobar su enlace y distribución, sin implementar todavía adquisición S7, configuración de PLC, escrituras ni SQL.

- `third_party/snap7/`: 35 archivos originales (755834 bytes), con fuentes, wrapper y licencias del adjunto. Comparación byte a byte contra el commit oficial `30f37da3114024a71ba93f7fd855c680b97a406f`; no se copiaron binarios, objetos ni `.git`.
- Núcleo Snap7 1.4.3; wrapper con encabezado histórico 1.4.0. SHA256SUMS y procedencia en UPSTREAM.md; `.gitattributes` conserva los bytes originales.
- `makefile`: compila biblioteca compartida nativa en `bin/lib/libsnap7.so`, wrapper C++ y enlace del driver; dependencias de cabeceras y limpieza de objetos. No requiere descargas ni instalación global de Snap7.
- El ejecutable declara `NEEDED: libsnap7.so` y usa `$ORIGIN/lib` para encontrarla después de trasladar la distribución. Las bibliotecas externas mantienen la configuración previa de cada entorno.
- `tools/package-runtime.sh`: incluye biblioteca reemplazable, licencias GPL/LGPL y `share/snap7-source.tar.gz` con fuentes correspondientes y receta; sin fuentes del driver ni configuración privada.
- Pruebas nuevas de integridad, ABI/estado independiente de clientes y distribución. `make test` las incorpora; `make test-snap7` permite ejecutarlas por separado.
- Documentación: `docs/siemens_s7.md`, guía de desarrollo, README y guía de pruebas.

## Verificación y evidencia

Entorno y overrides de compilación: mismos que la etapa 0. Dos trabajos de compilación. Las pruebas locales no consultan el PLC del usuario ni el esquema de aplicación.

```bash
cd /workspace/driver_modbus
source /workspace/driver-modbus-env/activate.sh
make -j2 test \
  INCLUDES='-Isrc/include -I/workspace/driver-modbus-env/sysroot/usr/include -I/workspace/driver-modbus-env/sysroot/usr/include/modbus' \
  LIBS='-L/workspace/driver-modbus-env/sysroot/usr/lib/x86_64-linux-gnu -Wl,-rpath,/workspace/driver-modbus-env/sysroot/usr/lib/x86_64-linux-gnu'
```

Resultado: código 0. Logs locales: `/workspace/driver-modbus-env/stage1-test.log` y `stage1-snap7.log`.

| Comprobación | Resultado |
|---|---|
| Compilación de Snap7 desde fuentes y enlace del driver | Aprobada en x86_64 |
| Manifiesto exacto de los 35 archivos upstream | Aprobado |
| Clientes independientes, parámetros y lectura desconectada | Aprobada; error real esperado, sin conexiones |
| Paquete trasladado, arranque --version y resolución de su propia biblioteca | Aprobado |
| Biblioteca corrupta en paquete temporal | Fallo de cargador esperado y detectado |
| Presencia de licencia/fuentes y ausencia de configuración privada | Aprobada |
| Regresión unitaria con ASan/UBSan y runner de estado | Aprobada |
| Modbus TCP/RTU local, RS232 y monitor | Aprobados |
| Correo simulado, mailcheck y cliente del monitor | 5, 3 y 4 pruebas Python aprobadas, respectivamente |
| Arranque sin licencia | 3 escenarios aprobados; indisponibilidad SQL esperada |
| make dist con los mismos overrides | Aprobado; log stage1-dist.log |
| Reconstrucción independiente de las fuentes Snap7 incluidas en el paquete | Aprobada |

Se comprobó la receta de reconstrucción sin modificar los fuentes vendorizados. La compilación y pruebas no presentaron fallos pendientes. Los mensajes del monitor sobre estado SQL corresponden a simulación de estado, no a integración con base de datos.

## Entorno reutilizable

Se actualizó y ejecutó la receta cloud para compilar esta rama con las dependencias locales y comprobar la versión, retirando referencias al antiguo orquesta y a objetos ARM versionados de main. Los campos `install_script` y `start_skill` se guardaron con confirmación del servicio. Los pasos de inicio distinguen comprobaciones sin SQL del arranque productivo, que no se ejecutó. El usuario debe revisar y guardar esos cambios en los ajustes y publicar el entorno para activar su versión; no se afirma publicación ni restauración en otra máquina.

## Límites y cierre

La biblioteca compila y se distribuye; todavía no hay parser S7, catálogo local de PLC ni adquisición integrada. No se modifican protocolos existentes, licencias de aplicación ni persistencia. No se ejecutó `make test-sql`, no se escribió a PLC y no se usó hardware industrial.

Raspberry Pi 5 ARM64 y la integración con el S7-1200 se verificarán en la etapa 6. Los ensayos independientes adjuntos ya cuentan con validación física del usuario.

Etapa 1 validada. El commit y resultado del push se informan tras crear el commit. Siguiente trabajo previsto: etapa 2 (direcciones y tipos), fuera del alcance de este cierre.
