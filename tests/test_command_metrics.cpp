// Comprueba ventana acotada, percentiles, aislamiento por origen y concurrencia.
#include "command_metrics.h"
#include <cassert>
#include <thread>
int main() {
    using namespace command_metrics;
    Metrics metrics;
    assert(metrics.snapshot(Source::Local, Stage::Execute).count == 0);
    for (uint64_t i = 1; i <= 300; ++i)
        metrics.record(Source::Local, Stage::Execute, i, i % 10 == 0);
    const auto sample = metrics.snapshot(Source::Local, Stage::Execute);
    assert(sample.count == 300 && sample.failures == 30 && sample.window == 256);
    assert(sample.maximum_us == 300 && sample.p50_us == 172 && sample.p95_us == 288);
    assert(metrics.snapshot(Source::External, Stage::Execute).count == 0);
    std::thread first([&] {
        for (int i = 0; i < 1000; ++i)
            metrics.record(Source::External, Stage::AckSql, 7);
        transport_trace().attempts = 3;
    });
    std::thread second([&] {
        for (int i = 0; i < 1000; ++i)
            metrics.record(Source::External, Stage::AckSql, 9, true);
    });
    first.join();
    second.join();
    const auto ack = metrics.snapshot(Source::External, Stage::AckSql);
    assert(ack.count == 2000 && ack.failures == 1000 && ack.maximum_us == 9);
    assert(transport_trace().attempts == 0);
    assert(metrics.next_execution() == 1 && metrics.next_execution() == 2);
    assert(metrics.summary().find("local.execute={n=300,fail=30") != std::string::npos);
    assert(field("a\n\"b\\c") == "\"a_\\\"b\\\\c\"");
    assert(field(std::string(1000, 'a')).size() == 101);
}
