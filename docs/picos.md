# Conservación de picos por tag

Por defecto se conserva la política del último valor al vencer la escritura
histórica. Las frecuencias de lectura y escritura no se modifican.

Para conservar también los extremos adquiridos durante cada intervalo, añadir
en la sección de configuración existente de `configdb.cfg`:

```ini
picos_tags = Equipo1/rVoltaje,Equipo2/rVoltaje
```

La lista usa los nombres exactos `dispositivo/tag`, separados por comas sin
espacios. Una lista vacía desactiva la opción. No habilitarla en temperatura
si solo interesa el último valor.

Al vencer el periodo histórico se guardan hasta tres muestras reales en
`tbvalores`: mínimo, máximo y última, ordenadas por adquisición y sin repetir
la misma muestra. Cada una conserva su fecha de adquisición y el tag original.
No se necesita migración SQL. La memoria por tag es constante.

Esto preserva extremos observados entre escrituras; no garantiza capturar
transitorios entre dos lecturas ni reconstruye todas las muestras del intervalo.
Los consumidores de históricos deben admitir varias filas del mismo tag y
segundo. El modo `solocambios` continúa aplicándose.
