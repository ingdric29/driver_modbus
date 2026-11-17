# Integración Siemens S7: dependencia y compilación

Estado: etapa 1. Snap7 está integrado en compilación y distribución; el driver
todavía no adquiere datos S7. No se añadieron direcciones, configuración de PLC,
escrituras, comandos nuevos ni reglas SQL. El alcance completo está en
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

## Distribución

El paquete contiene `bin/lib/libsnap7.so`, avisos en `licenses/snap7/` y fuentes
correspondientes con receta en `share/snap7-source.tar.gz`. La biblioteca puede
reemplazarse por una reconstrucción compatible de esa misma arquitectura/ABI.
Las demás bibliotecas del sistema siguen siendo requisitos externos.

Se validó Linux x86_64. La compilación y adquisición integrada en Raspberry Pi
5 ARM64 con el S7-1200 están pendientes de la etapa 6; las pruebas físicas
previas del usuario corresponden a los ensayos independientes adjuntos.
