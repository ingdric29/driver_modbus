// Verifica límites útiles, superposición y selección explícita de tags vencidos.
#include "s7_batch.h"
#include <cassert>
#include <iostream>
int main() {
    auto first = s7::parse_address("DB100.DBB0");
    auto second = s7::parse_address("DB100.DBB1");
    auto overlap = s7::parse_address("DB100.DBX0.7");
    auto gap = s7::parse_address("DB100.DBW4");
    auto other = s7::parse_address("DB101.DBB2");
    auto blocks = s7::plan({{0, first}, {1, second}, {2, overlap}, {3, gap}, {4, other}}, 222);
    assert(blocks.size() == 3 && blocks[0].count == 2 && blocks[0].tags.size() == 3);
    assert(blocks[1].db == 100 && blocks[1].start == 4 && blocks[2].db == 101);
    auto fast = s7::plan({{0, first}}, 222);
    assert(fast.size() == 1 && fast[0].count == 1 && fast[0].tags.size() == 1);
    std::vector<std::pair<size_t, s7::Address>> many;
    for (int i = 0; i < 1000; ++i)
        many.push_back({static_cast<size_t>(i), s7::parse_address("DB1.DBB" + std::to_string(i))});
    blocks = s7::plan(many, 222);
    assert(blocks.size() == 5);
    int total = 0;
    for (const auto &block : blocks) {
        assert(block.count <= 222);
        total += block.count;
    }
    assert(total == 1000);
    bool rejected = false;
    try {
        s7::plan({}, 3);
    } catch (const std::invalid_argument &) {
        rejected = true;
    }
    assert(rejected);
    std::cout << "PASS S7 due-only batching, gaps, DB boundaries, overlaps and payload limits\n";
}
