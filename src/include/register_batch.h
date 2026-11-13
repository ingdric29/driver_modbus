#ifndef DRIVER_REGISTER_BATCH_H
#define DRIVER_REGISTER_BATCH_H
#include "modbus_transport.h"
#include <vector>
#include <algorithm>
#include <cstdint>
#include <atomic>
#include <ctime>
#include <chrono>

struct RegisterRange { int address; int count; };
inline std::vector<RegisterRange> merge_register_ranges(std::vector<RegisterRange> ranges, int limit = 20) {
    std::sort(ranges.begin(), ranges.end(), [](RegisterRange a, RegisterRange b) { return a.address < b.address; });
    std::vector<RegisterRange> result;
    for (auto range : ranges) {
        if (range.address < 0 || range.count < 1 || range.count > limit || range.address > 65536 - range.count) continue;
        if (!result.empty()) {
            auto& last = result.back();
            int end = std::max(last.address + last.count, range.address + range.count);
            if (range.address <= last.address + last.count && end - last.address <= limit) { last.count = end - last.address; continue; }
        }
        result.push_back(range);
    }
    return result;
}
struct RegisterBlock {
    uint64_t session;
    int slave, function, address;
    std::vector<uint16_t> words;
    time_t acquired;
    std::chrono::steady_clock::time_point acquired_steady;
};
inline time_t& register_acquired() { static thread_local time_t acquired = 0; return acquired; }
inline std::chrono::steady_clock::time_point& register_acquired_steady() {
    static thread_local std::chrono::steady_clock::time_point acquired; return acquired;
}
inline std::vector<RegisterBlock>& register_blocks() { static thread_local std::vector<RegisterBlock> blocks; return blocks; }
struct RegisterCycle {
    RegisterCycle() { register_blocks().clear(); }
    ~RegisterCycle() { register_blocks().clear(); }
};
inline void prefetch_registers(ModbusLease& lease, int slave, int function, const std::vector<RegisterRange>& ranges) {
    if (!lease.get()) return;
    modbus_set_slave(lease.get(), slave == 0 ? 1 : slave);
    for (auto range : merge_register_ranges(ranges)) {
        RegisterBlock block{lease.session(), modbus_get_slave(lease.get()), function, range.address, std::vector<uint16_t>(range.count), 0, {}};
        const int received = function == 3
            ? modbus_read_registers(lease.get(), range.address, range.count, block.words.data())
            : modbus_read_input_registers(lease.get(), range.address, range.count, block.words.data());
        if (received == range.count) { block.acquired = time(nullptr); block.acquired_steady = std::chrono::steady_clock::now(); register_blocks().push_back(std::move(block)); }
        // A device may reject bulk reads; individual reader remains the fallback.
    }
}
inline int read_cached_registers(ModbusLease& lease, int function, int address, int count, uint16_t* output) {
    for (const auto& block : register_blocks()) {
        if (block.session == lease.session() && block.slave == modbus_get_slave(lease.get()) && block.function == function
            && address >= block.address && count >= 1 && address + count <= block.address + int(block.words.size())) {
            std::copy_n(block.words.begin() + (address - block.address), count, output);
            register_acquired() = block.acquired;
            register_acquired_steady() = block.acquired_steady;
            return count;
        }
    }
    const int result = function == 3 ? modbus_read_registers(lease.get(), address, count, output)
                                    : modbus_read_input_registers(lease.get(), address, count, output);
    if (result > 0) { register_acquired() = time(nullptr); register_acquired_steady() = std::chrono::steady_clock::now(); }
    return result;
}
#endif
