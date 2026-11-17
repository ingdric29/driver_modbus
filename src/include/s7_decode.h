/*
 * Archivo: s7_decode.h
 * Descripción: Valida direcciones DB y convierte valores Siemens big-endian.
 * Necesita: Bytes leídos desde el offset de la dirección y escalamiento finito.
 * Retornos: Dirección, tipo o double; lanza invalid_argument ante datos inválidos.
 */
#ifndef AUTOMONITOR_S7_DECODE_H
#define AUTOMONITOR_S7_DECODE_H
#include <cmath>
#include <cstdint>
#include <cstring>
#include <limits>
#include <stdexcept>
#include <string>
#include <sstream>
#include <iomanip>
#include <locale>

namespace s7 {
enum class Type { Bool, Byte, SInt, Word, Int, DWord, DInt, Real };
struct Address {
    int db = 0;
    int byte = 0;
    int bit = -1;
    int width = 0;
};
inline int decimal(const std::string &text, size_t &position, int maximum) {
    size_t start = position;
    int value = 0;
    while (position < text.size() && text[position] >= '0' && text[position] <= '9') {
        int digit = text[position++] - '0';
        if (digit > maximum || value > (maximum - digit) / 10)
            throw std::invalid_argument("S7 number out of range");
        value = value * 10 + digit;
    }
    if (position == start)
        throw std::invalid_argument("S7 missing unsigned decimal number");
    return value;
}
// DB numbers 1..65535; the S7ANY bit address has a 24-bit limit.
inline Address parse_address(const std::string &text) {
    if (text.size() > 64 || text.compare(0, 2, "DB") != 0)
        throw std::invalid_argument("S7 address must start with DB");
    Address address;
    size_t position = 2;
    address.db = decimal(text, position, 65535);
    if (!address.db || text.compare(position, 3, ".DB") != 0)
        throw std::invalid_argument("S7 invalid DB number or separator");
    position += 3;
    if (position >= text.size())
        throw std::invalid_argument("S7 missing DB width");
    char width = text[position++];
    switch (width) {
    case 'X':
    case 'B':
        address.width = 1;
        break;
    case 'W':
        address.width = 2;
        break;
    case 'D':
        address.width = 4;
        break;
    default:
        throw std::invalid_argument("S7 width must be X, B, W or D");
    }
    address.byte = decimal(text, position, 2097151);
    if (width == 'X') {
        if (position >= text.size() || text[position++] != '.')
            throw std::invalid_argument("S7 BOOL requires a bit");
        address.bit = decimal(text, position, 7);
    }
    if (position != text.size() || address.byte > 2097152 - address.width)
        throw std::invalid_argument("S7 trailing characters or range exceeds S7ANY");
    return address;
}
inline Type parse_type(const std::string &name) {
    const char *names[] = {"BOOL", "BYTE", "SINT", "WORD", "INT", "DWORD", "DINT", "REAL"};
    const char *legacy[] = {"s7_bool", "s7_byte",  "s7_sint", "s7_word",
                            "s7_int",  "s7_dword", "s7_dint", "s7_real"};
    for (int i = 0; i < 8; ++i)
        if (name == names[i] || name == legacy[i])
            return static_cast<Type>(i);
    throw std::invalid_argument("S7 unknown interpretation");
}
inline int width_of(Type type) {
    switch (type) {
    case Type::Bool:
    case Type::Byte:
    case Type::SInt:
        return 1;
    case Type::Word:
    case Type::Int:
        return 2;
    case Type::DWord:
    case Type::DInt:
    case Type::Real:
        return 4;
    }
    throw std::invalid_argument("S7 invalid type");
}
inline void validate(const Address &address, Type type) {
    if (address.db < 1 || address.db > 65535 || address.byte < 0 ||
        address.width != width_of(type) || address.byte > 2097152 - address.width ||
        (type == Type::Bool ? address.bit < 0 || address.bit > 7 : address.bit != -1))
        throw std::invalid_argument("S7 address and type are incompatible");
}
// Decimal text must recover the original binary32, not expose its binary64 expansion.
inline double real_decimal(float value) {
    if (!std::isfinite(value))
        throw std::invalid_argument("S7 non-finite REAL");
    for (int digits = 1; digits <= std::numeric_limits<float>::max_digits10; ++digits) {
        std::ostringstream out;
        out.imbue(std::locale::classic());
        out << std::setprecision(digits) << value;
        std::istringstream back(out.str());
        back.imbue(std::locale::classic());
        float recovered = 0;
        if (!(back >> recovered) || recovered != value ||
            std::signbit(recovered) != std::signbit(value))
            continue;
        std::istringstream decimal(out.str());
        decimal.imbue(std::locale::classic());
        double result = 0;
        if (decimal >> result)
            return result;
    }
    // Some standard libraries reject subnormal or boundary float input.
    return static_cast<double>(value);
}
// Shortest significant-digit count that preserves the normalized numeric value.
inline std::string value_text(double value) {
    if (!std::isfinite(value))
        throw std::invalid_argument("S7 non-finite value");
    if (std::trunc(value) == value && std::fabs(value) <= 1e15) {
        std::ostringstream out;
        out.imbue(std::locale::classic());
        out << std::fixed << std::setprecision(0) << value;
        return out.str();
    }
    std::string text;
    for (int digits = 1; digits <= std::numeric_limits<double>::max_digits10; ++digits) {
        std::ostringstream out;
        out.imbue(std::locale::classic());
        out << std::setprecision(digits) << value;
        text = out.str();
        std::istringstream back(text);
        back.imbue(std::locale::classic());
        double recovered = 0;
        if ((back >> recovered) && recovered == value &&
            std::signbit(recovered) == std::signbit(value))
            return text;
    }
    return text;
}
// Buffer begins at address.byte; bit numbering is Siemens 0..7 in that byte.
inline double decode(const Address &address, Type type, const uint8_t *bytes, size_t size,
                     double scale = 1) {
    validate(address, type);
    if (!bytes || size < static_cast<size_t>(address.width) || !std::isfinite(scale))
        throw std::invalid_argument("S7 short buffer or invalid scale");
    uint32_t raw = 0;
    for (int i = 0; i < address.width; ++i)
        raw = (raw << 8) | bytes[i];
    double value = raw;
    switch (type) {
    case Type::Bool:
        value = (raw >> address.bit) & 1;
        break;
    case Type::SInt:
        value = raw >= 128 ? static_cast<int>(raw) - 256 : static_cast<int>(raw);
        break;
    case Type::Int:
        value = raw >= 32768 ? static_cast<int>(raw) - 65536 : static_cast<int>(raw);
        break;
    case Type::DInt:
        value = raw >= 2147483648u ? static_cast<int64_t>(raw) - 4294967296LL : raw;
        break;
    case Type::Real: {
        static_assert(sizeof(float) == 4 && std::numeric_limits<float>::is_iec559,
                      "S7 REAL requires IEEE754 binary32");
        float real;
        std::memcpy(&real, &raw, sizeof(real));
        value = real_decimal(real);
        break;
    }
    default:
        break;
    }
    if (!std::isfinite(value) || !std::isfinite(value * scale))
        throw std::invalid_argument("S7 non-finite sample");
    return value * scale;
}
} // namespace s7
#endif
