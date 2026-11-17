# Etapa 3: transporte Snap7 de solo lectura

Fecha: 17 de noviembre de 2026. Rama: automonitor-modbus-driver-V1.2.0-pruebas.

`s7_transport.h/.cpp` posee un cliente no copiable por destino, protegido por mutex. Valida IPv4 numérica, puerto, rack 0..7, slot 0..31 y timeout 50..5000 ms (1000 por defecto). Se evita DNS para que la resolución no introduzca esperas ilimitadas. No expone operaciones de escritura.

Se reutiliza RetryBackoff (100 ms progresivos hasta 30 s). Las lecturas reutilizan conexión, distinguen espera/reintento, fallo TCP/ISO y error remoto de DB/dirección, y entregan bytes solo al completar toda la solicitud. Snap7 fragmenta lecturas grandes según la PDU negociada. Tamaño máximo por llamada: 64 KiB; direcciones dentro de S7ANY. Timestamp de finalización con reloj de pared; latencia monotónica. Los contadores distinguen intentos, conexiones, llamadas de lectura y errores; una llamada puede contener varias transacciones de red.

## Pruebas

- `make test-snap7` con overrides de etapa 0: aprobado; log stage3-s7-final.log en `/workspace/driver-modbus-env`.
- Servidor Snap7 real en loopback y puerto libre: lectura de 1024 bytes verificados, reutilización de sesión, DB inexistente, rango fuera del DB y recuperación sin reconectar por error remoto.
- Dos hilos comparten el transporte sin solicitudes simultáneas sobre el cliente.
- Detención/reinicio del servidor: fallo sin bytes, espera progresiva sin intento adicional prematuro y nueva sesión al recuperar.
- Peer TCP silencioso: el timeout corta la operación en menos de 2 s con timeout configurado de 150 ms.
- Desconexión explícita y reconstrucción de conexión: aprobadas.
- `make -j2 test` con los overrides locales: código 0, log stage3-test.log; regresión completa sin SQL aprobada. La ampliación del caso de peer silencioso se ejecutó además en el runner focalizado.

No se contactó con PLC real, no se escribieron áreas S7 y no se inició SQL. Las pruebas de servidores locales no sustituyen el ensayo físico posterior. No quedan fallos requeridos pendientes. Etapa 3 validada; commit/push y SHA se informan después de crearlos.
