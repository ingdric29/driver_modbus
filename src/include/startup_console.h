/*
 * Archivo: startup_console.h
 * Descripción: Alinea filas y unidades del resumen de arranque.
 * Necesita: Etiquetas, valores y salida estándar disponible.
 * Retornos: startup_row escribe y retorna void; startup_seconds devuelve texto con unidad.
 */

#ifndef AUTOMONITOR_STARTUP_CONSOLE_H
#define AUTOMONITOR_STARTUP_CONSOLE_H
#include <iostream>
#include <sstream>
#include <iomanip>

// Plain text works over SSH, in redirected logs and on Windows terminals.
inline void startup_row(const std::string &label, const std::string &value) {
    std::ostringstream line;
    line << "  " << std::left << std::setw(23) << label << value;
    std::cout << line.str() << std::endl;
}
template <class T> inline std::string startup_seconds(T value) {
    std::ostringstream text;
    text << value << " s";
    return text.str();
}
#endif
