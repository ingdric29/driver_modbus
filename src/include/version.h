/*
 * Archivo: version.h
 * Descripción: Define versión del producto y metadatos de compilación.
 * Necesita: Macros de commit/fecha aportadas por make o valores predeterminados.
 * Retornos: Constantes de texto; no realiza operaciones ni retorna un estado.
 */

#ifndef AUTOMONITOR_VERSION_H
#define AUTOMONITOR_VERSION_H
// Single source of the product version (monitor protocol version is separate).
#define AUTOMONITOR_VERSION "1.4.0"
#ifndef AUTOMONITOR_COMMIT
#define AUTOMONITOR_COMMIT "unknown"
#endif
#ifndef AUTOMONITOR_BUILD_DATE
#define AUTOMONITOR_BUILD_DATE "unknown"
#endif
#endif
