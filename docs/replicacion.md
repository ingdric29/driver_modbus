# Cursor de replicación

La sincronización usa el `id` de origen, ordenado y filtrado por concentrador.
Así no omite registros cuando varias muestras comparten fecha o llegan tarde.
Los datos se insertan con una transacción y el cursor avanza tras confirmar.

El checkpoint se guarda mediante reemplazo atómico en
`log/replication-<identidad>.cursor`, junto a la configuración. La identidad
incluye origen, destino, tabla y concentrador; no contiene credenciales.
Conservar ese archivo junto con la base local. Los IDs de origen deben ser
crecientes y no reutilizarse mediante TRUNCATE/restauraciones sin revisar el cursor.

Si el destino está vacío, una instalación sin checkpoint comienza desde cero.
Si ya contiene datos del concentrador, la sincronización se detiene para no
inferir un punto de avance incorrecto. Restaurar el checkpoint correspondiente
o configurar explícitamente en la sección existente de configdb.cfg:

```ini
replicacion_inicial_id = 12345
```

Ese número es el último ID **local** confirmado en destino, no el ID remoto.
Usar cero solo si se desea copiar desde el principio; puede duplicar datos ya
existentes. El valor solo se aplica cuando falta el checkpoint.

Una caída entre COMMIT y la escritura del checkpoint todavía puede duplicar
el último lote. Una garantía de entrega exactamente una vez exigiría identidad
de origen e idempotencia en el esquema remoto; esta versión no la promete.
