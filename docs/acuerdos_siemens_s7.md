# Acuerdos de integración Siemens S7 en AutoMonitor

Fecha: 17 de noviembre de 2026.

## Objetivo y alcance aprobado

Integrar comunicación Siemens S7 mediante Snap7 dentro del driver existente de ingdric29/driver_modbus. Conservar Modbus TCP, Modbus RTU y RS232. No crear un driver Siemens independiente.

- Rama de trabajo confirmada: `automonitor-modbus-driver-V1.2.0-pruebas`.
- Primera versión: exclusivamente lectura de PLC; sin escritura a PLC.
- Objetivo inicial: Siemens S7-1200 CPU 1215C, firmware 3.0.1; Raspberry Pi 5 ARM64 como plataforma de validación física.
- Área inicial: bloques DB con direccionamiento absoluto.
- Tipos: BOOL, BYTE, SINT, WORD, INT, DWORD, DINT y REAL.
- Configuración local para desarrollo y pruebas, sin pruebas SQL en estas etapas.
- Los ensayos adjuntos ya fueron probados por el usuario desde la Raspberry con ese PLC. Esto acredita esos ensayos, no la futura integración en el driver.
- Antes de implementar se acordó establecer reglas y etapas. La instalación de herramientas no inicia la implementación.

## Material de referencia

- `AutoMonitor_Siemens_V1_Diseno_Completo.pdf`: referencia técnica; sus instrucciones históricas no sustituyen los acuerdos actuales.
- `automonitor-s7.zip`: fuentes Snap7 y ensayos de conexión, lectura y escritura. El ensayo de escritura no forma parte del alcance autorizado de adquisición.
- `herramientas_entorno_cloud_2026-11-17.md`: inventario de otra sesión, contrastado con la máquina actual.

El PDF contemplaba un producto separado, C++17, YAML y persistencia propia. Esas decisiones no se trasladan automáticamente. La recomendación aceptada consiste en aprovechar primero los componentes del driver original y conservar C++11 salvo necesidad justificada y acordada.

## Reglas técnicas de trabajo

1. Reutilizar planificación, publicación de muestras, escalamiento, monitor, métricas, logs y ciclo de vida existentes donde sean compatibles.
2. Mantener el transporte S7 separado de los transportes Modbus y su decodificación específica.
3. Direcciones como `DB100.DBX0.0`, `DB100.DBB1`, `DB100.DBW2` y `DB100.DBD4`: ubicación/ancho separados de interpretación y signo. Validar combinaciones y límites.
4. Decodificación Siemens big-endian; probar valores negativos, límites y REAL no finitos.
5. Sesiones reutilizables por destino configurado, con acceso serializado; no crear conexiones por tag ni compartir clientes sin protección.
6. Timeouts acotados, reconexión progresiva y distinción entre errores de transporte y de dirección/configuración.
7. Un PLC desconectado o lento no debe bloquear Modbus ni los demás PLC S7. La estrategia concreta de trabajadores se definirá al implementar.
8. Agrupar solo lecturas compatibles vencidas; no adelantar periodos de otros tags. Respetar límites y PDU negociada.
9. Una lectura fallida es inválida: no fabricar ceros ni presentar una muestra antigua como adquisición nueva.
10. Fijar versión/commit de Snap7, revisar licencia y compilar para cada arquitectura objetivo.
11. Comprobar PUT/GET, protección y DB sin acceso optimizado según CPU/firmware. Snap7 no representa compatibilidad universal con Siemens ni PROFINET IO.
12. Simulación local y pruebas físicas son evidencias distintas. La integración final debe verificarse nuevamente en la Raspberry y el PLC.

## Etapas y criterios de cierre

| Etapa | Trabajo | Verificación requerida |
|---|---|---|
| 0. Línea base | Activar la rama acordada, revisar herramientas y ejecutar pruebas existentes sin SQL. | Compilación y pruebas base; identificar fallos previos sin atribuirlos a S7. |
| 1. Snap7 y compilación | Fijar dependencia e incorporar compilación/distribución. | Compilación con Snap7 y regresión existente aprobada. |
| 2. Direcciones y tipos | Parser DB, validación y decodificación. | Límites, negativos, REAL, direcciones inválidas y buffers insuficientes. |
| 3. Transporte S7 | Conexión, lectura, timeouts, reconexión y cierre. | Servidor Snap7 local, errores y recuperación controlados; sin escrituras a PLC. |
| 4. Adquisición integrada | Configuración local, periodos, escalamiento y publicación. | Coexistencia Modbus/S7, aislamiento entre PLC y rechazo de muestras inválidas. |
| 5. Optimización y operación | Agrupación/PDU, monitor, métricas, recarga y apagado. | Frecuencias, lecturas grandes y cierre/recarga sin recursos pendientes. |
| 6. Raspberry y PLC real | Compilar y validar la integración con el equipo objetivo. | Contraste de valores, desconexión/reconexión y ejecución sostenida. |

SQL queda aplazado; tener MariaDB instalado no autoriza ni exige pruebas de integración SQL del driver en este alcance.

## Informe, commit y publicación por etapa

Cada etapa genera un informe Markdown. Debe contener:

- Objetivo, alcance y cambios realizados.
- Archivos afectados y decisiones relevantes.
- Entorno, versiones, comandos y pruebas ejecutadas.
- Resultados: aprobadas, fallidas, omitidas y no ejecutadas, con evidencia pertinente.
- Limitaciones, riesgos y pendientes.
- Conclusión sobre cumplimiento de criterios y siguiente etapa.

Secuencia de cierre acordada:

1. Ejecutar pruebas de etapa y regresión pertinente.
2. Corregir fallos requeridos antes del cierre.
3. Preparar el informe y revisar el diff, excluyendo credenciales y archivos generados.
4. Crear commit local descriptivo que incluya el informe de etapa.
5. Hacer push a la misma rama y comprobar el SHA remoto.
6. Informar SHA local, resultado del push y ubicación del informe; avanzar una vez cumplido el cierre.

El SHA y la comprobación remota pueden figurar en el mensaje de cierre o en un registro posterior: no se exige insertar en un commit su propio SHA. Si falla el push, declarar que el commit solo está local y no presentar la etapa como sincronizada. No se acordó mezclar a main ni publicar una release.

## Estado al exportar

La rama remota confirmada apunta a `9a04c039ef75d47af480687baacb388f072768bb` en la inspección realizada. La rama activa del checkout no se cambió durante la revisión de herramientas. No se ha iniciado la etapa 0 ni se ha implementado S7.

Las dependencias locales están en `/workspace/driver-modbus-env`, no en la ruta histórica `/workspace/.driver-env`. Se preparó `activate.sh` para exponer herramientas locales en cada shell. Los scripts iniciales de onboarding proceden de main y no deben usarse para compilar la rama reorganizada sin revisarlos en la etapa 0.

Los próximos detalles se resolverán con evidencia del código: formato final de configuración local, identidad de sesión por destino, parámetros de timeout y estrategia de concurrencia. No se fijan aún cuotas, tolerancias, colas ni reglas SQL nuevas tomadas del PDF.
