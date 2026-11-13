#include "modbus_decode.h"
#include <cassert>
#include <limits>
#include <iostream>
int main() {
    const uint16_t words[] = {0x3210, 0x7654, 0xba98, 0xfedc};
    assert(decode_u32_le(words) == UINT32_C(0x76543210));
    assert(decode_u32_be(words) == UINT32_C(0x32107654));
    assert(decode_u64_le(words) == UINT64_C(0xfedcba9876543210));
    const uint16_t maximum[] = {0xffff, 0xffff, 0xffff, 0xffff};
    assert(decode_u32_le(maximum) == UINT32_MAX);
    assert(decode_u64_le(maximum) == UINT64_MAX);
    assert(numeric_to_int(12.9) == 12);
    assert(numeric_to_int(-12.9) == -12);
    assert(numeric_to_int(1e100) == std::numeric_limits<int>::max());
    assert(numeric_to_int(-1e100) == std::numeric_limits<int>::min());
    assert(numeric_to_int(std::numeric_limits<double>::quiet_NaN()) == 0);
    assert(numeric_to_int(std::numeric_limits<double>::infinity()) == 0);
    std::cout << "11 decoder and integer conversion checks passed\n";
}
