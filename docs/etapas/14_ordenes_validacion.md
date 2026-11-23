# Órdenes Modbus — etapa 4: validación antes de transmitir

Fecha: 2026-11-23 (America/Caracas). Rama: `automonitor-driver-V1.3.0`.
Base: etapa 3, commit `4a23561`.

Actualización posterior: el usuario aprobó sustituir el rechazo global del lote
por [grupos independientes de dispositivo](15_ordenes_grupos_dispositivo.md).
Este informe describe la etapa 4 original; la política vigente está en ese ajuste.

## Cambios

El lote privado se valida completo antes del primer FC06. Si contiene una
configuración inválida o un ID duplicado, se rechaza entero y no se actualizan
las confirmaciones de sus comandos. Esto evita enviar sus primeras entradas
válidas antes de detectar un error posterior de configuración.

Cada comando comprueba:

- ID positivo y dispositivo habilitado en la configuración cargada.
- Dirección Modbus de 0 a 65535, tal como se almacena: no se interpreta ni resta
  automáticamente el prefijo documental 4xxxx.
- Valor WORD de 0 a 65535 o INT16 de -32768 a 32767. Los negativos se codifican
  explícitamente como palabra de 16 bits en complemento a dos.
- Protocolo MBTCP o MBSERIAL. No se habilita escritura Siemens ni otro protocolo.
- En TCP: IPv4 numérica, puerto 1..65535 y unidad 0..255, incluido el 255 usado
  por el PLC de referencia.
- En RTU: interfaz no vacía, velocidad positiva, paridad N/E/O (vacía equivale
  a N y se admiten minúsculas), uno o dos bits de parada y esclavo 0..247.
  La disponibilidad de la interfaz y velocidades admitidas se comprueban al
  abrir el transporte. Se conservan los ocho bits de datos existentes.
- Se conserva la normalización heredada de esclavo 0 a 1; el driver no realiza
  escrituras broadcast. TCP y RTU siguen usando conexiones y exclusión por enlace.

El lector SQL detecta NULL y conversiones inválidas en campos numéricos
necesarios para el transporte. No convierte un registro o valor NULL en cero.
El parser decimal evita truncamientos, redondeo o desbordamiento; admite signo,
espacios externos y fracciones formadas solo por ceros. Valida el valor ya
almacenado: no puede recuperar un decimal que SQL haya convertido antes a INT.

Los campos propios de otro transporte no invalidan una orden: por ejemplo,
un TCP no necesita velocidad serie. Los comandos inválidos del catálogo de
alarmas se conservan para poder identificarlos, pero se rechazan al ejecutarlos.

`cls_cmd::ejecutar` y los helpers públicos TCP/RTU comprueban la configuración
antes de abrir el enlace. `hab_ejecutar` mantiene su significado existente como
filtro de lotes manuales; una alarma `CMD[id]` es una referencia explícita y no
se desactiva por ese indicador. Sí debe estar habilitado su dispositivo.

## Alarmas y monitor

- Una acción CMD inexistente queda sin confirmar; se corrige su anterior marca
  de éxito por defecto. Se continúa procesando el EMAIL de esa misma alarma.
- Un CMD rechazado por validación tampoco impide intentar su correo.
- Las notificaciones ya aceptadas conservan sus marcas y no se repiten al
  reevaluar esas acciones fallidas en las pruebas.
- `COMMAND_METRICS` se muestra como INFO/METRICAS. Los resultados normales de
  órdenes se muestran como INFO/ORDENES; rechazo de validación, fallo Modbus o
  confirmación SQL fallida conservan ERROR/ORDENES.
- Los nombres de lote y motivos entre comillas no alteran la clasificación.
- Se añade la etapa de medición `validation` y `validation_us` al diagnóstico.

No se cambia la plantilla, configuración, autenticación ni transporte SMTP.
La agregación heredada por tipo de acción sigue sin resultados persistentes
por destinatario o por cada CMD de una lista; ese rediseño no pertenece a esta etapa.

## Verificación

- Parser con ASan/UBSan: fracciones reales, texto vacío, infinitos, exponentes,
  desbordamientos, límites INT y fracciones nulas.
- Driver enlazado: rechazos sin abrir conexión, unidad TCP 255 y registro 65535,
  valores 128/-1/-32768 con palabras 128/65535/32768, RTU real en pseudoterminal
  con paridad minúscula y normalización del esclavo cero.
- Evaluación pública de alarmas: CMD inválido e inexistente permiten EMAIL,
  conservan CMD pendiente y no duplican el correo ya aceptado.
- Binario real con SQL temporal: ocho lotes con primera entrada válida y segunda
  inválida (NULL, direcciones y valores fuera de rango); no se transmite ninguna.
- Dispositivo deshabilitado rechazado y límites INT16/WORD aceptados en FC06.
- Continúan los casos de 130 comandos, Latin-1/NULL, límite 4096, IDs externos
  distintos, correo de activación/desactivación/reactivación, adquisición mixta
  Siemens/Modbus y cierre.
- Monitor: métricas informativas y errores reales correctamente diferenciados.

Resultado aprobado: `bash install.sh --skip-packages --test-command-sql
--test-s7-sql` completó sus diez pasos. La compilación terminó sin warnings;
pasaron la regresión ordinaria, la prueba SQL mixta y ambos perfiles de órdenes.
También se repitió el runner enlazado con ASan/UBSan y terminó sin errores
de memoria ni comportamiento indefinido. El test de acciones de alarma usa
su API pública `evaluarcompleto`, conservando privados sus helpers internos.

Se verificaron seis mensajes aceptados por SMTP TLS temporal (tres por perfil),
además de los casos ordinarios de rechazo del servidor y reintento. Las medianas
orientativas de tres órdenes fueron 156,0 ms y 146,7 ms, con período local de
200 ms. Estos valores no establecen un límite de latencia físico. La configuración
local conservó el mismo SHA256 de las etapas anteriores y no se ejecutó APT.

Para compilar y repetir con dependencias previamente instaladas:

```bash
bash install.sh --skip-packages --test-command-sql --test-s7-sql
```

## Límites conservados

Esta validación previa no convierte el lote en una transacción atómica del PLC:
un fallo de comunicación después de algunos envíos aún puede dejar ejecución
parcial. No hay readback obligatorio ni confirmación de efecto físico final.
Continúan el consumo anticipado de solicitudes, los tres intentos de transporte
y el seguimiento SQL heredado. La etapa 5 quedó pendiente expresamente por
decisión del usuario; no se crean tablas, estados ni migraciones de producción.

El aislamiento completo de SMTP y la planificación concurrente por enlace de
la etapa 6 siguen pendientes. Las validaciones cloud usan exclusivamente
servidores temporales y pseudoterminales, sin acceder a los equipos reales.
