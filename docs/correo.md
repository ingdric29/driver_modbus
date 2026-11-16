# Correo de alarmas

Una alarma activa y una notificación entregada son estados diferentes.
Los errores cURL 28 (timeout) y 35 (negociación TLS) indican que el intento
falló; por sí solos no identifican una contraseña incorrecta ni una entrega
en spam. Comprobar primero la conexión y el modo TLS desde la Raspberry.

## Diagnóstico sin enviar mensajes

`make` instala `bin/automonitor-mailcheck`. Usa Python 3 y su biblioteca
estándar. Puede ejecutarse con el driver trabajando, sin otra adquisición:

```bash
cd ~/driver_modbus
./bin/automonitor-mailcheck --ports 465,587
```

Lee solamente la URL `smtp_servidor` de `configdb.cfg`; no lee ni utiliza
credenciales SMTP. Comprueba conexión, saludo SMTP, EHLO y TLS con verificación
del certificado. No ejecuta AUTH, MAIL FROM, RCPT TO ni DATA. La opción
`--ports 465,587` compara TLS implícito en 465 con STARTTLS en 587 sobre el
mismo servidor; no modifica la configuración. Sin esa opción prueba la URL
configurada, con su protocolo y puerto exactos.

También se puede indicar una ruta o un servidor confirmado por el proveedor:

```bash
./bin/automonitor-mailcheck --config /ruta/configdb.cfg
./bin/automonitor-mailcheck --server smtps://smtp.example.invalid:465
./bin/automonitor-mailcheck --server smtp://smtp.example.invalid:587
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

## Rutina de envío original restaurada

Se toma directamente la rutina `enviar_email` del commit inicial `0c80b7f`,
incluida la construcción del mensaje y las opciones SMTP de libcurl:

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
Se retiran `smtp_ca_file` y la imposición de STARTTLS añadidos durante los
intentos de reparación. El emisor vuelve a utilizar la URL tal como la usaba
el original. La configuración restaurada usa SMTPS para cifrar el envío.

Se restaura la lectura de los cuatro campos SMTP y sus valores por defecto
del cargador original. Los valores presentes en configdb.cfg se mantienen,
al igual que todos los campos de base de datos.
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
El modo `EMISOR_ORIGINAL limites=libcurl_original` identifica esta restauración.
`INFO: SMTP: ... aceptado por el servidor` confirma la aceptación del mensaje;
no garantiza su llegada a la bandeja de entrada. Si persiste `smtp=535`, el
servidor está rechazando AUTH y la restauración no permite afirmar que el
correo quedó resuelto. No publicar contraseñas para comprobar ese resultado.

## Equivalencia con el original

Se restauran la fecha y las cabeceras originales, el formato multipart/related,
la plantilla y sus sustituciones, Cc y la declaración de contenido del emisor
inicial. Las diferencias que se conservan son el callback acotado al buffer,
el resultado de error si falla la inicialización de cURL y el registro compacto
de los resultados sin activar trazas de autenticación ni volcados del cuerpo.

La prueba compila tanto el emisor actual como una copia sin modificaciones de
`0c80b7f:src/enviarmail.cpp`, guardada en `tests/fixtures/original_enviarmail.cpp`.
Ambos envían a un servidor TLS de loopback con credenciales ficticias y una
fecha fija. Se comparan la autenticación SASL, MAIL FROM, RCPT TO y los bytes
del mensaje completo, con y sin Cc. Solo se normaliza Message-ID, que es único
en cada envío. El resultado de ambas versiones debe coincidir exactamente.
También se prueban una descripción de 120 kB con ASan/UBSan, una negociación
TLS lenta, AUTH rechazado y rechazo tras DATA. Ningún correo sale a Internet.
La aceptación real en la Raspberry sigue necesitando el resultado del servidor.
