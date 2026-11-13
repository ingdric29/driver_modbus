#include "register_batch.h"
#include <cassert>
int main() {
    auto ranges = merge_register_ranges({{12, 2}, {10, 2}, {13, 2}, {30, 1}});
    assert(ranges.size() == 2);
    assert(ranges[0].address == 10 && ranges[0].count == 5);
    assert(ranges[1].address == 30 && ranges[1].count == 1);
    ranges = merge_register_ranges({{0, 20}, {20, 1}});
    assert(ranges.size() == 2);
    assert(merge_register_ranges({{-1, 1}, {65535, 2}, {0, 0}}).empty());
    {
        RegisterCycle cycle;
        auto lease = ModbusLease::tcp("127.0.0.1", 1); // Allocates a context; never connects.
        modbus_set_slave(lease.get(), 1);
        const auto acquired = std::chrono::steady_clock::now() - std::chrono::seconds(2);
        register_blocks().push_back({lease.session(), 1, 3, 0, {5}, 100, acquired});
        uint16_t value = 0;
        assert(read_cached_registers(lease, 3, 0, 1, &value) == 1 && value == 5);
        assert(register_acquired() == 100 && register_acquired_steady() == acquired);
    }
    assert(register_blocks().empty());
}
