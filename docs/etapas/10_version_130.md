# AutoMonitor Driver 1.3.0

Por petición del usuario se establece versión 1.3.0 sin RC ni sufijo de pruebas,
y se renombra la rama local existente a automonitor-driver-V1.3.0 con git branch
-m, conservando todo el historial. Identidad: AutoMonitor Driver / INDUSTRIAL
DRIVER. Los ejecutables mantienen sus nombres para conservar servicios/scripts.
Se actualizan README, guía de Raspberry y CHANGELOG, distinguiendo funcionalidad
implementada de alarmas/eventos Siemens y almacenamiento persistente pendientes.
No se crea etiqueta ni release.

Validación mediante install.sh --skip-packages con dependencias cloud: compilación,
regresión ordinaria, versiones, distribución y limpieza. No se cambian adquisición
ni persistencia ni la configuración privada.

El renombrado remoto mediante API de GitHub devolvió Forbidden incluso con la
ruta de ejecución ampliada. No se crea una segunda rama ni se elimina la anterior.
El commit se publica sobre la rama remota existente. Para completar el renombrado,
el usuario debe usar Rename branch en GitHub para cambiar
 automonitor-modbus-driver-V1.2.0-pruebas a automonitor-driver-V1.3.0.
La publicación del commit no acredita que ese renombrado remoto se haya realizado.
