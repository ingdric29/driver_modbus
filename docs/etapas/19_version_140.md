# Nueva rama y versión 1.4.0

Fecha: 25 de noviembre de 2026 (Caracas).
Base: `e70d2bd`, integración de alarmas y eventos Siemens.
Rama nueva de trabajo: `automonitor-driver-V1.4.0`.
La rama anterior `automonitor-driver-V1.3.0` se conserva en su estado previo.

## Cambios

Versión única en src/include/version.h: 1.4.0, utilizada por driver y visor.
README actualizado con la nueva rama y comandos de instalación; CHANGELOG con
el alcance acumulado. Se conservan nombres de ejecutables, configuración, esquema
SQL, dependencias compartidas y lógica de adquisición, órdenes, alarmas y correo.
No se crea una etiqueta de lanzamiento ni se renombra la rama anterior.

Incluye Siemens DB de solo lectura, persistencia SQL, alarmas/eventos Siemens y
expresiones mixtas, mejoras de órdenes Modbus y contadores E/P/W/R. La validación
de activación/desactivación Siemens en la Raspberry sigue pendiente de confirmación.

## Verificación

Compilar con las dependencias cloud existentes mediante Make (sin instalar
paquetes), comprobar automonitorv1 --version y automonitorctl --version, y ejecutar
la regresión del visor. No se repite la suite PLC/SQL completa: no hay cambios de
lógica desde e70d2bd, que pasó el instalador con los perfiles SQL/SMTP.

Resultado: PASS. Compilación completa y ambos ejecutables muestran 1.4.0.
Las cuatro pruebas del visor y git diff --check pasaron. No se cambió configdb.cfg.

## Actualizar la Raspberry

La carpeta del checkout puede conservar su nombre anterior; la rama de Git es
independiente del nombre del directorio. Desde el checkout utilizado:

```bash
cd "$HOME/driver_modbus_V1.3.0" &&
git fetch origin &&
git switch automonitor-driver-V1.4.0 &&
git pull --ff-only origin automonitor-driver-V1.4.0 &&
bash install.sh --skip-packages
```

El instalador compila el checkout; no reemplaza automáticamente el bin de la
instalación activa. Desplegar bin completo, incluido bin/lib, con el driver
anterior detenido y conservando configdb.cfg como hasta ahora.
