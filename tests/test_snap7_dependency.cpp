// Stage 1: exercise the linked Snap7 ABI without connecting to a PLC.
#include "snap7.h"
#include <iostream>
#include <stdexcept>

static void require(bool condition, const char *message) {
    if (!condition)
        throw std::runtime_error(message);
}

int main() {
    try {
        TS7Client first;
        TS7Client second;
        require(!first.Connected() && !second.Connected(), "unexpected connection");
        int first_timeout = 1250;
        int second_timeout = 2750;
        require(first.SetParam(p_i32_RecvTimeout, &first_timeout) == 0, "set first timeout");
        require(second.SetParam(p_i32_RecvTimeout, &second_timeout) == 0, "set second timeout");
        int actual = 0;
        require(first.GetParam(p_i32_RecvTimeout, &actual) == 0 && actual == first_timeout,
                "first client state not isolated");
        require(second.GetParam(p_i32_RecvTimeout, &actual) == 0 && actual == second_timeout,
                "second client state not isolated");
        unsigned char data[2] = {};
        const int error = first.ReadArea(S7AreaDB, 100, 0, 2, S7WLByte, data);
        require(error != 0, "disconnected read unexpectedly succeeded");
        require(!CliErrorText(error).empty(), "missing error description");
        require(!first.Connected(), "read established an unexpected connection");
        std::cout
            << "PASS Snap7 ABI: independent clients, parameters, disconnected read and cleanup\n";
    } catch (const std::exception &error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
