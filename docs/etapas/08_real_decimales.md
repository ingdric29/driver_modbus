# Corrección de expansión decimal de REAL Siemens

Fecha: 17 de noviembre de 2026. Rama automonitor-modbus-driver-V1.2.0-pruebas.

El usuario confirmó escrituras físicas en ambas tablas y mostró test_real=25.6 en TIA y 25.600000381469727 en SQL. El patrón binary32 0x41CCCCCD representa aproximadamente 25.6000003814697265625. La promoción directa a double y el formato de 17/12 cifras exponían esa aproximación como precisión aparente adicional.

## Corrección

Para REAL se busca la representación decimal con el menor número de cifras significativas que, leída como float, recupera el mismo valor y signo originales. Se usa como valor numérico double de la muestra antes del escalamiento. No se fija un número global de decimales ni se redondean los tipos enteros. Si la biblioteca estándar rechaza la lectura de un valor extremo/subnormal, se conserva su promoción original sin pérdida.

El monitor y valorstr comparten ahora un formateador decimal de ida/vuelta para el valor normalizado; valorfloat recibe ese mismo número mediante setDouble. Los enteros de magnitud normal se muestran sin notación científica. Para representaciones SQL mayores a los 20 caracteres disponibles, valorstr conserva una representación compacta de 12 cifras como antes, sin estrechar valorfloat.

Resultado comprobado para la muestra reportada, escala 1:

- Monitor: 25.6.
- tbinstantaneos.valorstr / valorfloat: 25.6 / 25.6.
- tbvalores.valorstr / valorfloat: 25.6 / 25.6.

No se cambia ninguna tabla, rastro, dirección ni interpretación Modbus. El escalamiento continúa usando aritmética double; esto no introduce una política de decimales de ingeniería para todos los factores ni permite recuperar una cantidad de decimales de TIA que no viaja en el protocolo. DOUBLE sigue siendo un formato binario; un consumidor que fuerce precisión excesiva puede mostrar su representación binaria. La precisión fuente sigue siendo la de REAL de 32 bits.

## Verificación

- Unitario con ASan/UBSan y -Werror: patrón exacto 25.6, 0.1, negativos, enteros máximos DWORD, valor vecino representable distinto, ocho cifras significativas, valores pequeños, subnormal, máximo finito y cero negativo. Se conserva distinción entre valores REAL cercanos; no se aplana todo a uno/dos decimales.
- make test: regresión ordinaria completa, adquisición local mixta y empaquetado.
- make test-s7-sql en MariaDB temporal: se agrega comando del fixture que cambia exclusivamente la memoria simulada a los bytes 0x41CCCCCD. El driver real debe mostrar 25.6 y las consultas a ambas tablas comprueban simultáneamente valorstr y valorfloat iguales a 25.6. También se conserva la regresión SQL de rollback/reintento, cambios e independencia de Modbus.

Logs locales: s7-real-regression.log y s7-real-sql.log bajo /workspace/driver-modbus-env. Pruebas contra simuladores/DB aislada; el nuevo binario aún no fue ejecutado en la Raspberry del usuario.

Los históricos ya escritos no se modifican: no se pueden redondear masivamente sin una política explícita de precisión por tag. Los nuevos instantáneos se actualizan con las siguientes lecturas tras desplegar esta versión; los nuevos históricos siguen sus periodos y solocambios habituales. Commit/push y SHA se informan después del cierre.
