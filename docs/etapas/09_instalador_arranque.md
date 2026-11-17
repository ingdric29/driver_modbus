# Instalador único y resumen de Siemens al iniciar

Se mantiene install.sh como vía recomendada para dependencias, compilación,
pruebas y distribución; no se crea otro instalador. La opción --test-s7-sql añade
la dependencia MariaDB Server/Client y la prueba mixta aislada al flujo y su barra
de progreso. --skip-packages permite el mismo flujo con dependencias locales
preparadas, omitiendo sudo/APT. Los errores siguen deteniendo el paso y conservan
su código. No se modifica configdb.cfg ni se inicia adquisición al terminar.

Se aclara que la distribución lleva biblioteca, licencias y fuentes de Snap7,
sin fuentes de aplicación ni configuración privada. La documentación de uso
actual remite al instalador; los comandos make de informes históricos se
conservan como evidencia de pruebas anteriores.

El resumen ADQUISICION añade Tags Siemens S7 y PLC Siemens S7 con los conteos
reales cargados desde SQL para el concentrador configurado. Aparecen antes del
mensaje de trabajadores iniciados. El caso sin Siemens muestra cero.

Validación: bash -n; ejecución real de bash install.sh --skip-packages
--test-s7-sql usando las dependencias extraídas del cloud, INCLUDES/LIBS y
S7_TEST_SYSROOT. Se comprueba compilación, regresión, prueba SQL mixta, versiones,
distribución y limpieza; la integración SQL verifica también tres tags y un PLC
en el texto de arranque. Log local: /workspace/driver-modbus-env/installer-s7.log.
APT/sudo no están disponibles en esta imagen: su rama no se ejecuta aquí y la
instalación real de paquetes debe verificarse en Raspberry. La adquisición y
persistencia no cambian en esta entrega. No se modifica la base del usuario.
