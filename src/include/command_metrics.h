/*
 * Archivo: command_metrics.h
 * Descripción: Mide órdenes con reloj monotónico y memoria acotada.
 * Necesita: Duraciones y resultados; no almacena credenciales ni estados persistentes.
 * Retornos: Instantáneas y resúmenes; percentiles de las últimas 256 operaciones por etapa.
 */

#ifndef DRIVER_COMMAND_METRICS_H
#define DRIVER_COMMAND_METRICS_H
#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <mutex>
#include <sstream>
#include <string>
#include <vector>

namespace command_metrics {
using Clock = std::chrono::steady_clock;
inline uint64_t elapsed_us(Clock::time_point start) {
    return std::chrono::duration_cast<std::chrono::microseconds>(Clock::now() - start).count();
}
enum class Source { Local, External };
enum class Stage { MutexWait, PollSql, CatalogSql, Execute, AckSql, Batch, PollLag, Count };
inline const char *source_name(Source source) {
    return source == Source::Local ? "local" : "externa";
}
struct Snapshot {
    uint64_t count = 0, failures = 0, maximum_us = 0, p50_us = 0, p95_us = 0;
    size_t window = 0;
};
class Metrics {
    struct Samples {
        uint64_t count = 0, failures = 0, maximum_us = 0;
        std::array<uint64_t, 256> recent{};
    };
    std::mutex mutex_;
    std::array<std::array<Samples, static_cast<size_t>(Stage::Count)>, 2> samples_{};
    std::atomic<uint64_t> sequence_{0};

  public:
    static Metrics &instance() {
        static Metrics metrics;
        return metrics;
    }
    uint64_t next_execution() { return ++sequence_; }
    void record(Source source, Stage stage, uint64_t us, bool failed = false) {
        std::lock_guard<std::mutex> lock(mutex_);
        auto &sample = samples_[static_cast<size_t>(source)][static_cast<size_t>(stage)];
        sample.recent[sample.count % sample.recent.size()] = us;
        ++sample.count;
        sample.failures += failed;
        sample.maximum_us = std::max(sample.maximum_us, us);
    }
    Snapshot snapshot(Source source, Stage stage) {
        std::lock_guard<std::mutex> lock(mutex_);
        const auto &sample = samples_[static_cast<size_t>(source)][static_cast<size_t>(stage)];
        Snapshot result;
        result.count = sample.count;
        result.failures = sample.failures;
        result.maximum_us = sample.maximum_us;
        result.window = std::min<uint64_t>(sample.count, sample.recent.size());
        if (result.window) {
            std::vector<uint64_t> sorted(sample.recent.begin(),
                                         sample.recent.begin() + result.window);
            std::sort(sorted.begin(), sorted.end());
            result.p50_us = sorted[(result.window * 50 + 99) / 100 - 1];
            result.p95_us = sorted[(result.window * 95 + 99) / 100 - 1];
        }
        return result;
    }
    std::string summary() {
        static const char *names[] = {"mutex",   "poll_sql", "catalog_sql", "execute",
                                      "ack_sql", "batch",    "poll_lag"};
        std::ostringstream out;
        out << "COMMAND_METRICS units=us window_limit=256";
        for (auto source : {Source::Local, Source::External}) {
            for (size_t stage = 0; stage < static_cast<size_t>(Stage::Count); ++stage) {
                const auto value = snapshot(source, static_cast<Stage>(stage));
                if (!value.count)
                    continue;
                out << ' ' << source_name(source) << '.' << names[stage] << "={n=" << value.count
                    << ",fail=" << value.failures << ",window=" << value.window
                    << ",p50=" << value.p50_us << ",p95=" << value.p95_us
                    << ",max=" << value.maximum_us << '}';
            }
        }
        return out.str();
    }
};

// Observation of the last cls_cmd::ejecutar on this thread, also safe for alarm workers.
struct TransportTrace {
    uint64_t link_wait_us = 0, connect_us = 0, wire_us = 0;
    unsigned attempts = 0;
    int error = 0;
};
inline TransportTrace &transport_trace() {
    static thread_local TransportTrace trace;
    return trace;
}
inline std::string field(const std::string &value) {
    std::string result = "\"";
    for (size_t i = 0; i < value.size() && i < 96; ++i) {
        const auto c = static_cast<unsigned char>(value[i]);
        if (c == '\\' || c == '"')
            result += '\\';
        result += c < 32 || c == 127 ? '_' : static_cast<char>(c);
    }
    if (value.size() > 96)
        result += "...";
    return result + '"';
}
} // namespace command_metrics
#endif
