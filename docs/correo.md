# Correo de alarmas

Una alarma activa y una notificación entregada son estados diferentes.
Los errores cURL 28 (timeout) y 35 (negociación TLS) indican que el intento
falló; por sí solos no identifican una contraseña incorrecta ni una entrega
en spam. Comprobar primero la conexión y el modo TLS desde la Raspberry.

## Diagnóstico sin enviar mensajes

`make` instala `.build/automonitor-mailcheck`. Usa Python 3 y su biblioteca
estándar. Puede ejecutarse con el driver trabajando, sin otra adquisición:

```bash
cd ~/driver_modbus
./.build/automonitor-mailcheck --ports 465,587
```

Lee solamente la URL `smtp_servidor` de `configdb.cfg`; no lee ni utiliza
credenciales SMTP. Comprueba conexión, saludo SMTP, EHLO y TLS con verificación
del certificado. No ejecuta AUTH, MAIL FROM, RCPT TO ni DATA. La opción
`--ports 465,587` compara TLS implícito en 465 con STARTTLS en 587 sobre el
mismo servidor; no modifica la configuración. Sin esa opción prueba la URL
configurada, con su protocolo y puerto exactos.

También se puede indicar una ruta o un servidor confirmado por el proveedor:

```bash
./.build/automonitor-mailcheck --config /ruta/configdb.cfg
./.build/automonitor-mailcheck --server smtps://smtp.example.invalid:465
./.build/automonitor-mailcheck --server smtp://smtp.example.invalid:587
```

Usar el servidor real, no `example.invalid`. No incluir usuario o contraseña
en la URL. Por defecto cada operación de red tiene un límite de seis segundos;
`--timeout` permite entre uno y treinta. Las comprobaciones DNS dependen del
resolvedor del sistema. El comando devuelve 0 si alguna opción funciona, 1
si ninguna funciona y 2 si falta configuración válida.

| Resultado | Revisar |
|---|---|
| DNS | Nombre del servidor y resolvedor de la Raspberry |
| Conexión rechazada / timeout | Puerto, firewall/ruta, servicio y posible bloqueo de IP |
| Negociación TLS | Concordancia entre puerto y TLS implícito/STARTTLS |
| Certificado no válido | Nombre del servidor, fecha del sistema, cadena y CA instaladas |
| TLS correcto | Credenciales, remitente autorizado y respuesta final del envío |

Un TLS correcto confirma acceso al servidor, no autenticación ni entrega en
la bandeja de entrada. Solo cambiar `smtp_servidor` por una opción compatible
con las instrucciones del proveedor. La configuración SMTP se carga al iniciar
el driver: modificarla requiere un reinicio controlado de esa instancia.

## Conexión original restaurada

A petición del usuario se restaura la conexión del emisor original (`0c80b7f`):

```ini
smtp_servidor=smtps://p3plzcpnl463001.prod.phx3.secureserver.net
```

La URL SMTPS sin puerto usa el 465 de libcurl, como antes. Se revierte la
sustitución por STARTTLS/587 del commit `f6f22ae` en la configuración versionada.
La prueba anterior por 587 confirmó TLS, pero los registros posteriores
mostraron `codigo=67 smtp=535 etapa=AUTH`: no confirmó autenticación ni entrega.

El emisor deja de establecer CONNECTTIMEOUT y TIMEOUT. Se retiran del cargador
las opciones `smtp_timeout_conexion` y `smtp_timeout_total` añadidas durante
el diagnóstico; si permanecen en un archivo local, se ignoran. Se conservan
los tiempos predeterminados de libcurl (conexión predeterminada de la biblioteca
y sin límite total añadido por la aplicación). Una operación de correo en
curso puede prolongar el cierre del proceso, como en el emisor original.

Se restablece también la política TLS heredada: con la configuración original
no se verifica la cadena del certificado (`SSL_VERIFYPEER=0`), pero continúa
la comprobación del nombre (`SSL_VERIFYHOST=2`). SMTPS sigue cifrando la conexión.
Si se proporciona `smtp_ca_file`, se activa la verificación de la cadena con
ese archivo CA. Para una URL `smtp://` configurada expresamente se mantiene
STARTTLS obligatorio; no se envían credenciales en texto plano.

Las credenciales SMTP y los campos de base de datos existentes no cambian.
Se mantienen los resultados de error correctos, el buffer de subida seguro,
los logs por alarma y la aceptación por el servidor como condición de éxito.
La traza SMTP verbosa permanece desactivada para no volcar autenticación ni
cuerpos de mensajes en la consola del driver.

## Actualización y comprobación

```bash
cd ~/driver_modbus
git pull origin mejoras-driver-pasos-1-2
make -j2
```

Comprobar la URL SMTP de la configuración local si se editó manualmente.
Detener la instancia existente con SIGINT/SIGTERM y arrancar una sola instancia
actualizada desde `.build`. No ejecutar el binario nuevo encima del existente.

El log mantiene alarma, dispositivo, directorio, resultado cURL/SMTP y tiempos.
El modo `SMTPS_ORIGINAL limites=libcurl_original` identifica esta restauración.
`INFO: SMTP: ... aceptado por el servidor` confirma la aceptación del mensaje;
no garantiza su llegada a la bandeja de entrada. Si persiste `smtp=535`, el
servidor está rechazando AUTH y la restauración no permite afirmar que el
correo quedó resuelto. No publicar contraseñas para comprobar ese resultado.

## Validación local

Se comprueban subida grande, envío SMTPS con negociación TLS demorada cuatro
segundos sin límites de aplicación, política de certificado original, CA
explícita, AUTH rechazado y rechazo tras DATA. Las alarmas con error conservan
su envío pendiente y las aceptadas no se repiten. Las pruebas usan TLS en
loopback y destinatarios example.invalid, sin correo externo.
