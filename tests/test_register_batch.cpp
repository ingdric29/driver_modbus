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
    { RegisterCycle cycle; register_blocks().push_back({1, 1, 3, 0, {5}}); assert(register_blocks().size() == 1); }
    assert(register_blocks().empty());
}
