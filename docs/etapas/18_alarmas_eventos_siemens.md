# Integración de alarmas y eventos con Siemens

Fecha: 25 de noviembre de 2026 (Caracas).
Rama: `automonitor-driver-V1.3.0`. Base: `ab68904`.

## Resultado y alcance

Las muestras Snap7 del arranque SQL se conectan al evaluador existente de
alarmas y a los eventos. La comunicación Siemens sigue siendo de solo lectura.
Se reutilizan `tbdefalarmas`, `tbdefeventos`, `tbdirectorio`, `tbhistoalarmas`,
`tbhistoeventos` y las acciones existentes. No hay migraciones de estructura,
cambios de página ni nuevos parámetros de configuración. No se insertan
alarmas, destinatarios ni eventos en la base productiva automáticamente.

Soporta los ocho tipos S7 ya disponibles. Las alarmas reciben el valor escalado
como double y resuelven `$(dispositivo.tag)`, incluidas expresiones mixtas con
Modbus. Eventos soportan valores completos (incluido REAL/DWORD), BOOL y bits
hasta 31 según tipo. CMD puede dirigirse a los destinos Modbus existentes;
no se añade escritura Snap7. EMAIL reutiliza directorio, plantilla y transporte
SMTP existentes, incluyendo ambas transiciones.

## Causa del recorrido incompleto

La adquisición S7 compartía muestras con SQL y monitor; alarmas y eventos solo
buscaban en arreglos TCP/RTU/RS232. No encontraban las variables Siemens. Usar
el arreglo Modbus como adaptación habría introducido truncamiento/float y
mezclado interpretaciones, por lo que se añadió un registro S7 de muestras.

## Implementación

- `s7_runtime_samples.h`: registro por nombre exacto de dispositivo/tag, muestras
  compartidas con SQL, snapshot bajo mutex y metadatos de origen, DB, tipo y unidad.
- Se publica valor bruto y escalado, fecha y reloj monótono desde la lectura
  existente, sin otra lectura de PLC. El registro se reemplaza antes de arrancar
  trabajadores y se limpia después de unirlos al liberar el catálogo.
- Alarmas conservan búsqueda anterior y agregan búsqueda S7. Expresiones Siemens
  conservan precisión, incluso REAL 1e-7, sin pasar por float ni redondear a seis
  decimales antes de evaluar. Históricos/correos mantienen sus formatos heredados.
- Las muestras inválidas, ausentes, en reconexión o más antiguas que
  `max(1 segundo, 3 × período)` no evalúan la referencia. La alarma conserva su
  estado; monitor la marca sin evaluar. No se sustituyen por cero.
- Eventos resuelven referencias antes de iniciar adquisición/evaluación. Su
  primera muestra válida fija baseline, sin transición de arranque. Un fallo
  no altera el baseline. Tras recuperar, se compara con el último valor válido.
- Los bits usan el entero bruto previo al escalamiento. No se usan los campos
  heredados de 16 bits para comparar DWORD/DINT o REAL. El camino anterior de
  Modbus/RS232 permanece separado.
- Selector incompatible (por ejemplo bit 0 de REAL) deja ese evento sin resolver
  y produce un aviso al cargar; no interrumpe los demás eventos ni alarmas.
- SQL y SMTP se ejecutan fuera del bloqueo de muestra. No hay consultas SQL ni
  operaciones PLC dentro de la búsqueda de una referencia.

## Configuración

Ver [guía de Siemens](../siemens_s7.md#alarmas-y-eventos-siemens-modo-sql).
Se requieren definiciones reales en las tablas existentes, con nombre de
`tbdispositivos` y tag de `tbregistros` exactamente iguales a los adquiridos.
No se usan IDs de registros Modbus ni su columna `registro` para resolver S7.

Alarmas usan las expresiones existentes, por ejemplo:

```text
activar:    $(Siemens_S7_1200.test_real)>30
desactivar: $(Siemens_S7_1200.test_real)<29
acción de cada transición: EMAIL[ID_DEL_DIRECTORIO];
```

Para eventos: `bit1=-1` representa valor completo (obligatorio para REAL).
BOOL admite -1 o 0, BYTE/SINT bits 0..7, WORD/INT 0..15 y DWORD/DINT 0..31.
Para BOOL/bits: mensajes activado/desactivado para 1/0. Cambios completos
numéricos usan el texto desactivado seguido del valor, conservando la convención
anterior; configurar un texto apropiado como «Valor actualizado».

## Pruebas y herramientas

Herramientas: install.sh/Make, C++, ASan/UBSan para unitarias, Python, libmodbus,
Snap7 con servidor loopback, MariaDB temporal y SMTP TLS/AUTH temporal.

```bash
bash install.sh --skip-packages --test-command-sql --test-s7-sql
```

Cloud utiliza INCLUDES/LIBS/S7_TEST_SYSROOT de `/workspace/driver-modbus-env`.
No se utilizaron credenciales ni hardware productivos. Nuevas comprobaciones:

- Snapshot S7: validez, antigüedad, precisión REAL, entero escalado y bit 31,
  concurrencia de publicación y reemplazo del catálogo.
- Driver enlazado: alarma REAL 1e-7, metadatos S7, falta de validez/antigüedad
  sin desactivación y referencia retirada.
- Driver completo con SQL/Snap7/Modbus/SMTP: BOOL, REAL, DWORD máximo, bit 31,
  cambios de 65536 a 131072 (idénticos 16 bits bajos), REAL 25.6 sin truncar,
  histórico y correo de activación/desactivación/reactivación.
- Alarma S7 ejecuta CMD Modbus de 128/0, comprobados mediante lectura del destino.
  Otro CMD ausente no impide los correos de ambas transiciones de una alarma BOOL.
- Expresión mixta, caída Siemens sin falsas transiciones, alarma Modbus
  independiente durante la caída, recuperación y eliminación S7 por recarga.
- Evento REAL con bit incompatible: aviso y ninguna fila histórica.
- Regresión de lecturas, históricos, monitor, órdenes Modbus, correos local/externo,
  reconexiones y empaquetado/licencias mediante el instalador habitual.

Estado final: **PASS**, instalador con 10/10 pasos y salida 0. Pasaron las
unitarias ASan/UBSan, driver enlazado, adquisición local, Snap7, SQL mixto,
perfil nuevo de alarmas/eventos y los tres perfiles de órdenes Modbus
(local, externo, recuperación). Configuración privada conservada sin cambios
(SHA-256 igual al estado inicial).

Evidencia cloud: `/workspace/driver-modbus-env/s7-alarms-installer-final.log`.
La prueba nueva verificó tres correos REAL, dos BOOL pese a CMD ausente,
un correo de expresión mixta y uno Modbus durante la caída Siemens. La prueba
SQL no usa los historiales físicos previos del usuario como evidencia de esta
función nueva.

## Límites y pendientes

- Falta la validación con las definiciones reales y el S7-1200 de la Raspberry.
- Los períodos existentes siguen vigentes: alarmas 60 s por defecto, eventos
  4 s por defecto y lectura según tbregistros.periodo. No se cambia su planificación.
- Se procesan últimas muestras, no todas las transiciones internas del PLC.
  Cambios breves entre lecturas/revisiones pueden perderse; el PLC debe retener
  señales o contar pulsos si se necesita detectar cada uno.
- Las referencias de distintos tags/PLC no forman un snapshot atómico conjunto.
- Correo pendiente, entrega exactamente una vez, deduplicación persistente,
  seguimiento persistente de órdenes y escrituras S7 continúan fuera del alcance.
- Reinicio/recarga conservan las reglas anteriores de inicialización de alarmas:
  pueden volver a notificar una condición activa. Los eventos S7 establecen una
  referencia nueva. No se retiene un historial de cambios ocurridos sin adquisición.
- Fallos de almacenamiento de eventos no adelantan el baseline: se reintenta el
  último cambio disponible, sin cola persistente ni garantía contra duplicados
  ante confirmación SQL perdida.

## Consultas útiles de solo lectura en Raspberry

```bash
sudo mariadb --table dbdevlog <<'SQL'
SELECT a.id, a.dispositivo, a.tag, a.activar, a.desactivar,
       a.accionactivar, a.acciondesactivar, a.valorAlarma, a.habilitar
FROM tbdefalarmas a JOIN tbdispositivos d ON d.nombre=a.dispositivo
WHERE d.protocolo='SIEMENS_S7';
SELECT e.id, e.nombredispositivo, e.tag, e.bit1,
       e.activado, e.desactivado, e.habilitar
FROM tbdefeventos e JOIN tbdispositivos d ON d.nombre=e.nombredispositivo
WHERE d.protocolo='SIEMENS_S7';
SELECT id, id_alarma, fechatiempo, tipo, dispositivo, tag, valor
FROM tbhistoalarmas ORDER BY id DESC LIMIT 20;
SELECT id_defeventos, fechatiempo, nombredispositivo, tag, bit1, mensaje
FROM tbhistoeventos ORDER BY fechatiempo DESC LIMIT 20;
SQL
```

Consultar el visor desde la instalación con `./bin/automonitorctl --once`.
Configurar/revisar destinatarios y definiciones, reiniciar el driver y probar
condiciones autorizadas del PLC, verificando ambos históricos y ambos correos.
