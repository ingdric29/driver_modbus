# Operación y diagnóstico

SIGINT y SIGTERM solicitan parada. Las esperas del driver se interrumpen en
intervalos de hasta 20 ms; las operaciones de red en curso terminan o alcanzan
su timeout antes de unir trabajadores. No hay garantía de apagado en 20 ms.
SQL utiliza límites de conexión/lectura/escritura de tres segundos. El correo
recupera los tiempos predeterminados de libcurl, sin límite total añadido
por el driver; una operación SMTP en curso puede retrasar el cierre. Un cierre pierde históricos no confirmados
que aún estén en memoria; las métricas muestran las ventanas pendientes.

Las conexiones Modbus fallidas aplican espera progresiva desde 100 ms hasta
30 s por endpoint/puerto. Esto no cambia los periodos de los tags sanos. Se
puede configurar `modbus_timeout_us` entre 1000 y 3000000, por defecto 90000.
El límite de reintentos por lectura sigue siendo tres.

Los registros `METRICS` indican llamadas de lectura, fallos, duración máxima,
conexiones creadas/reutilizadas, muestras confirmadas, fallos SQL, muestras
replicadas y ventanas pendientes. Son contadores del proceso; las llamadas
de lectura incluyen intentos suspendidos por backoff. No son garantías de
frecuencia ni latencia máxima bajo carga. Se emiten desde el ciclo de comandos
cuando han transcurrido 60 s, y al finalizar el coordinador.

Los errores se publican con el estado del dispositivo. Se retiró la escritura
específica del antiguo diagnóstico sobre `wAlarmas04` con conexión y credenciales
fijas, para que el driver utilice exclusivamente su configuración de datos.
RTU y RS232 directo no pueden configurarse en la misma interfaz a la vez.

Una carga de configuración inválida o fallida impide iniciar trabajadores;
el arranque no requiere licencia ni está vinculado a las interfaces de red.
Los parámetros antiguos `clave1` y `clave2` se ignoran y pueden retirarse de
configdb.cfg. La autenticación configurada para bases de datos y correo sigue
siendo necesaria cuando esos servicios la exigen.

El inicio del proceso se registra en `log/devlog.log`. No se inserta como una
alarma de dispositivo en `tbhistoalarmas`, porque ese evento global no tiene
un dispositivo asociado. Las alarmas reales siguen guardando su dispositivo;
no se desactivan ni se modifican las restricciones de clave foránea.

El directorio y los destinatarios se cargan antes de las alarmas. Cada acción
EMAIL consulta el directorio vigente y prepara una copia del mensaje; no
conserva punteros a un directorio de una generación anterior. Si el ID no
existe, la acción devuelve error y no se marca como enviada.

El [monitor SSH](monitor.md) permite consultar en vivo el estado de cada
dispositivo/tag, alarmas, SQL y eventos desde otra terminal, sin iniciar otra
adquisición. Tras instalar esta versión, el driver debe reiniciarse una vez
para habilitar el socket local; abrir/cerrar visores posteriores no requiere reinicio.

Para problemas de envío de alarmas, ver [diagnóstico y configuración SMTP](correo.md).
