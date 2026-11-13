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

## Configuración de envío corregida

La prueba realizada desde la Raspberry del usuario mostró timeout en 465 y
STARTTLS con certificado válido en 587 (5,10 segundos). Para esa instalación:

```ini
smtp_servidor = smtp://p3plzcpnl463001.prod.phx3.secureserver.net:587
smtp_timeout_conexion = 10
smtp_timeout_total = 30
```

El archivo versionado incorpora estos tres valores. Si mantienes una
configuración local diferente, conserva tus parámetros de base de datos,
remitente, usuario y contraseña, y aplica únicamente esas líneas SMTP.

El transporte pide STARTTLS obligatorio con `smtp://` y TLS implícito con
`smtps://`. Verifica la cadena del certificado y el nombre del servidor;
no degrada el envío a texto plano. `smtp_ca_file` permite indicar un archivo
CA para una instalación privada; vacío utiliza las CA del sistema.
Los límites de conexión y envío son configurables de 1 a 300 segundos;
el tiempo total debe ser mayor o igual al de conexión. Los valores por
defecto son 10 y 30 segundos. El saludo, TLS y autenticación consumen parte
del tiempo total. No se usan credenciales predeterminadas ocultas cuando
faltan campos SMTP; la adquisición sigue funcionando aunque falle el correo.

Cada intento registra alarma, dispositivo, ID del directorio, activación o
desactivación, modo TLS, resultado cURL, respuesta SMTP y marcas de tiempo
acumuladas de DNS, conexión, TLS y operación total. Los detalles de cURL
ocultan el usuario y la clave configurados. Un resultado de autenticación
fallida se identifica como etapa AUTH; un fallo de TLS mantiene la alarma
pendiente de notificar, nunca como correo enviado.

Una línea `INFO: SMTP: ... aceptado por el servidor` confirma que el servidor
aceptó el mensaje, no que llegó a la bandeja de entrada. Si aparece
`codigo=67 smtp=535 etapa=AUTH`, comprobar las credenciales y las condiciones
de acceso del proveedor sin publicarlas en el chat. Tras un fallo, las
alarmas que continúan cumpliendo su condición vuelven a intentar el envío
en su siguiente evaluación; una aceptación evita repetir el correo de esa
activación. Cada alarma conserva su estado independiente.

La construcción del mensaje usa una fecha RFC en inglés con zona horaria,
contenido HTML UTF-8 de 8 bits y omite Cc si está vacío. La subida respeta el
tamaño del buffer de cURL incluso con plantillas/descripciones grandes.

Actualizar y compilar, detener la instancia antigua con SIGINT/SIGTERM y
arrancar el binario nuevo desde `.build`. Cambiar solo la URL del archivo no
basta si se conserva el transporte anterior sin STARTTLS explícito.

## Regresión identificada en el historial

El commit `e3c54ed` (paso 11) añadió un límite de conexión de tres segundos y
uno total de diez; el emisor original no fijaba esos límites. Ese cambio podía
interrumpir conexiones legítimas lentas. El commit `f6f22ae` eleva los valores
por defecto a 10/30 segundos, permite configurarlos y añade STARTTLS explícito.
La prueba de regresión retrasa cuatro segundos la negociación TLS del mismo
servidor local: con el límite antiguo devuelve cURL 28 y con 10/30 completa
la autenticación y el envío. Esto reproduce el efecto del límite; confirmar
la aceptación real en la Raspberry requiere el resultado posterior al reinicio.
