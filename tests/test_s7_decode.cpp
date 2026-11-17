// Validación de dirección, límites y decodificación, sin Snap7 ni hardware.
#include "s7_decode.h"
#include <cassert>
#include <functional>
#include <iostream>
#include <vector>

static void rejected(const std::function<void()> &action) {
    bool failed = false;
    try {
        action();
    } catch (const std::invalid_argument &) {
        failed = true;
    }
    assert(failed);
}
static double value(const char *address, const char *type, std::vector<uint8_t> data,
                    double scale = 1) {
    return s7::decode(s7::parse_address(address), s7::parse_type(type), data.data(), data.size(),
                      scale);
}
int main() {
    for (const char *bad :
         {"", "DB0.DBB0", "DB65536.DBB0", "DB1.DBX0.8", "DB1.DBX0", "DB1.DBB-1", "DB1.DBW2097151",
          "DB1.DBD2097149", "DB1.DBQ0", "DB1.DBB0.0", " DB1.DBB0", "DB1.DBB0x", "DB1.DBX0.-1",
          "DB9999999999999999999999999999999.DBB0"})
        rejected([&] { s7::parse_address(bad); });
    assert(s7::parse_address("DB65535.DBD2097148").byte == 2097148);
    assert(s7::parse_address("DB1.DBB2097151").byte == 2097151);
    rejected([] { s7::parse_type("FLOAT"); });
    for (int bit = 0; bit < 8; ++bit) {
        auto address = s7::parse_address("DB100.DBX0." + std::to_string(bit));
        uint8_t data = uint8_t(1u << bit);
        assert(s7::decode(address, s7::Type::Bool, &data, 1) == 1);
        data = uint8_t(~data);
        assert(s7::decode(address, s7::Type::Bool, &data, 1) == 0);
    }
    assert(value("DB1.DBB1", "BYTE", {255}) == 255);
    assert(value("DB1.DBB1", "SINT", {128}) == -128);
    assert(value("DB1.DBB1", "SINT", {127}) == 127);
    assert(value("DB1.DBB1", "SINT", {255}) == -1);
    assert(value("DB1.DBW2", "WORD", {255, 255}) == 65535);
    assert(value("DB1.DBW2", "INT", {128, 0}) == -32768);
    assert(value("DB1.DBW2", "INT", {127, 255}) == 32767);
    assert(value("DB1.DBD4", "DWORD", {255, 255, 255, 255}) == 4294967295.0);
    assert(value("DB1.DBD4", "DINT", {128, 0, 0, 0}) == -2147483648.0);
    assert(value("DB1.DBD4", "DINT", {127, 255, 255, 255}) == 2147483647.0);
    assert(value("DB1.DBD4", "DINT", {255, 255, 255, 255}) == -1);
    assert(value("DB1.DBD4", "REAL", {0x41, 0xc8, 0, 0}) == 25);
    assert(value("DB1.DBD4", "s7_real", {0xc1, 0xc8, 0, 0}, -2) == 50);
    assert(value("DB1.DBD4", "REAL", {0, 0, 0, 0}, 0) == 0);
    rejected([] { value("DB1.DBD4", "REAL", {0x7f, 0x80, 0, 0}); });
    rejected([] { value("DB1.DBD4", "REAL", {0xff, 0x80, 0, 0}); });
    rejected([] { value("DB1.DBD4", "REAL", {0x7f, 0xc0, 0, 0}); });
    rejected([] { value("DB1.DBD4", "REAL", {0, 0, 0}); });
    rejected([] { value("DB1.DBD4", "INT", {0, 0, 0, 0}); });
    rejected([] { value("DB1.DBB1", "BOOL", {1}); });
    rejected([] { value("DB1.DBX0.0", "BYTE", {1}); });
    rejected([] { value("DB1.DBB1", "BYTE", {1}, INFINITY); });
    rejected([] { value("DB1.DBD4", "DWORD", {255, 255, 255, 255}, 1e308); });
    rejected([] { s7::decode(s7::parse_address("DB1.DBB0"), s7::Type::Byte, nullptr, 1); });
    std::cout << "PASS S7 addresses, eight types, boundaries and finite scaling\n";
}
