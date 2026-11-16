#ifndef MODBUS_DECODE_H
#define MODBUS_DECODE_H

#include <cstdint>
#include <cmath>
#include <limits>

inline uint32_t decode_u32_le(const uint16_t *words) {
    return uint32_t(words[0]) | (uint32_t(words[1]) << 16);
}
inline uint32_t decode_u32_be(const uint16_t *words) {
    return (uint32_t(words[0]) << 16) | uint32_t(words[1]);
}
inline uint64_t decode_u64_le(const uint16_t *words) {
    return uint64_t(words[0]) | (uint64_t(words[1]) << 16) | (uint64_t(words[2]) << 32) |
           (uint64_t(words[3]) << 48);
}
// Avoid undefined floating-point to integer conversions in legacy integer views.
inline int numeric_to_int(double value) {
    if (!std::isfinite(value))
        return 0;
    if (value >= std::numeric_limits<int>::max())
        return std::numeric_limits<int>::max();
    if (value <= std::numeric_limits<int>::min())
        return std::numeric_limits<int>::min();
    return static_cast<int>(value);
}
#endif
