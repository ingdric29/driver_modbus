// S7 snapshots: precision, validity, freshness, bit width and generation isolation.
#include "s7_runtime_samples.h"
#include <cassert>
#include <thread>
#include <atomic>
int main() {
    auto source = std::make_shared<s7_runtime::Sample>();
    s7_runtime::Snapshot snapshot;
    assert(!source->read(snapshot));
    const auto now = s7_runtime::Clock::now();
    source->latest.sequence = 1; source->latest.acquired = 100;
    source->latest.value = 4294967295.; source->latest.text = "4294967295";
    source->raw = 4294967295.; source->type = s7::Type::DWord;
    source->completed = now; source->period = .1; source->valid = true;
    assert(source->read(snapshot, now));
    assert(s7_runtime::event_value(snapshot, 31) == 1);
    assert(s7_runtime::event_value(snapshot, -1) == 4294967295.);
    assert(!source->read(snapshot, now + std::chrono::seconds(2)));
    source->valid = false; assert(!source->read(snapshot, now));
    source->valid = true; source->raw = -1; source->type = s7::Type::DInt;
    assert(source->read(snapshot, now) && s7_runtime::event_value(snapshot, 31) == 1);
    source->raw = 65536; source->latest.value = 6553.6;
    assert(source->read(snapshot, now) && s7_runtime::event_value(snapshot, 16) == 1);
    assert(s7_runtime::event_value(snapshot, -1) == 6553.6);
    source->type = s7::Type::Real; source->latest.value = 1e-7;
    assert(source->read(snapshot, now) && s7_runtime::event_value(snapshot, -1) == 1e-7);
    bool rejected = false;
    try { s7_runtime::event_value(snapshot, 0); } catch (const std::invalid_argument &) { rejected = true; }
    assert(rejected);
    auto &registry = s7_runtime::Registry::instance();
    registry.replace({{{"plc", "tag"}, source}});
    assert(registry.find("plc", "tag") == source && !registry.find("other", "tag"));
    registry.replace({}); assert(!registry.find("plc", "tag"));
    // Copies remain owned, and readers cannot observe a partial acquisition publication.
    std::atomic<bool> running{true};
    std::thread producer([&] {
        for (int i = 1; i <= 1000; ++i) {
            std::lock_guard<std::mutex> lock(source->mutex);
            source->latest.value = i; source->latest.text = std::to_string(i);
            source->raw = i; source->completed = s7_runtime::Clock::now();
        }
        running = false;
    });
    while (running) {
        if (source->read(snapshot)) {
            assert(snapshot.latest.value == snapshot.raw || snapshot.latest.value == 1e-7);
        }
    }
    producer.join();
}
