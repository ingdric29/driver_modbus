/*
 * Archivo: command_integer.h
 * Descripción: Convierte enteros SQL sin truncamiento, desbordamiento ni redondeo.
 * Necesita: Texto decimal; admite signo, espacios externos y fracción solo de ceros.
 * Retornos: true con entero exacto o false sin modificar el destino.
 */
#ifndef DRIVER_COMMAND_INTEGER_H
#define DRIVER_COMMAND_INTEGER_H
#include <cstdint>
#include <limits>
#include <string>

inline bool command_integer(const std::string &text, int &result) {
    const auto first = text.find_first_not_of(" \t\r\n");
    if (first == std::string::npos)
        return false;
    const auto end = text.find_last_not_of(" \t\r\n") + 1;
    size_t at = first;
    bool negative = false;
    if (text[at] == '-' || text[at] == '+')
        negative = text[at++] == '-';
    const auto digits = at;
    const uint64_t limit = static_cast<uint64_t>(std::numeric_limits<int>::max()) + negative;
    uint64_t value = 0;
    while (at < end && text[at] >= '0' && text[at] <= '9') {
        const unsigned digit = text[at++] - '0';
        if (value > (limit - digit) / 10)
            return false;
        value = value * 10 + digit;
    }
    if (at == digits)
        return false;
    if (at < end && text[at] == '.') {
        const auto decimal = ++at;
        while (at < end && text[at] == '0')
            ++at;
        if (at == decimal)
            return false;
    }
    if (at != end)
        return false;
    result = negative && value == limit ? std::numeric_limits<int>::min()
             : negative                 ? -static_cast<int>(value)
                                        : static_cast<int>(value);
    return true;
}
#endif
