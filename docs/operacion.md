# Operación y diagnóstico

SIGINT y SIGTERM solicitan parada. Las esperas del driver se interrumpen en
intervalos de hasta 20 ms; las operaciones de red en curso terminan o alcanzan
su timeout antes de unir trabajadores. No hay garantía de apagado en 20 ms.
SQL utiliza límites de conexión/lectura/escritura de tres segundos, y correo
un timeout total de diez segundos. Un cierre pierde históricos no confirmados
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
una licencia inválida sigue deteniendo el programa. El formato incorrecto de
la clave devuelve un error controlado y no modifica la validación de licencia.
