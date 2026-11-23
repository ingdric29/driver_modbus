// Rechazo exacto de texto numérico sin truncar ni redondear direcciones o valores.
#include "command_integer.h"
#include <cassert>
#include <limits>
int main() {
    int value = 99;
    for (const auto &text :
         {"", "NULL", "128.5", "1.00000000000000000001", "1e2", "0x80", "1 2", "--1", "1.", ".0",
          "+", "nan", "inf", "2147483648", "-2147483649", "99999999999999999999999999"}) {
        assert(!command_integer(text, value) && value == 99);
    }
    assert(command_integer("128", value) && value == 128);
    assert(command_integer(" +128.000 \t", value) && value == 128);
    assert(command_integer("-32768", value) && value == -32768);
    assert(command_integer("65535", value) && value == 65535);
    assert(command_integer("-0", value) && value == 0);
    assert(command_integer("2147483647", value) && value == std::numeric_limits<int>::max());
    assert(command_integer("-2147483648", value) && value == std::numeric_limits<int>::min());
}
