/*
 * Archivo: modbus_conversions.cpp
 * Descripción: Convierte palabras Modbus según el orden de bytes heredado.
 * Necesita: Puntero a al menos dos palabras uint16_t.
 * Retornos: Valor float; conserva exactamente las conversiones existentes.
 */

#include "include/conversiones.h"
#include <cstring>
#include <stdexcept>
#include <iomanip>
#include <locale>

using namespace std;

/**
 * Interpreta dos palabras mediante el orden de bytes de la variante.
 * Parámetros: const uint16_t *src
 * Retorno: Float resultante; requiere al menos dos palabras válidas.
 */
float cast_get_float_abcd(const uint16_t *src) {
    float f;
    uint32_t i;

    i = ntohl(((uint32_t)src[0] << 16) + src[1]);
    memcpy(&f, &i, sizeof(float));

    return f;
}

/**
 * Interpreta dos palabras mediante el orden de bytes de la variante.
 * Parámetros: const uint16_t *src
 * Retorno: Float resultante; requiere al menos dos palabras válidas.
 */
float cast_get_float_dcba(const uint16_t *src) {
    float f;
    uint32_t i;
    // windows  _byteswap_ulong()
    // linux bswap_32()

#ifdef _WIN32
    i = ntohl(_byteswap_ulong((((uint32_t)src[0]) << 16) + src[1]));
#else
    i = ntohl(bswap_32((((uint32_t)src[0]) << 16) + src[1]));
#endif

    memcpy(&f, &i, sizeof(float));
    return f;
}

/**
 * Interpreta dos palabras mediante el orden de bytes de la variante.
 * Parámetros: const uint16_t *src
 * Retorno: Float resultante; requiere al menos dos palabras válidas.
 */
float cast_get_float_badc(const uint16_t *src) {
    float f;
    uint32_t i;
    // windows _byteswap_ushort
    // linux bswap_16

#ifdef _WIN32
    i = ntohl((uint32_t)(_byteswap_ushort(src[0]) << 16) + _byteswap_ushort(src[1]));
#else
    i = ntohl((uint32_t)(bswap_16(src[0]) << 16) + bswap_16(src[1]));
#endif

    memcpy(&f, &i, sizeof(float));

    return f;
}

/**
 * Interpreta dos palabras mediante el orden de bytes de la variante.
 * Parámetros: const uint16_t *src
 * Retorno: Float resultante; requiere al menos dos palabras válidas.
 */
float cast_get_float_cdab(const uint16_t *src) {
    float f;
    uint32_t i;

    i = ntohl((((uint32_t)src[1]) << 16) + src[0]);
    memcpy(&f, &i, sizeof(float));

    return f;
}
