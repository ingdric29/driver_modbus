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
