/* Muestras S7 compartidas por adquisición, SQL, alarmas y eventos.
 * Registro por generación: reemplazar/limpiar solo con trabajadores detenidos. */
#ifndef AUTOMONITOR_S7_RUNTIME_SAMPLES_H
#define AUTOMONITOR_S7_RUNTIME_SAMPLES_H
#include "sample_window.h"
#include "s7_decode.h"
#include <chrono>
#include <map>
#include <memory>
#include <mutex>
namespace s7_runtime {
using Clock = std::chrono::steady_clock;
struct Snapshot {
    SampleValue latest;
    double raw = 0;
    std::string origin, address, unit;
    s7::Type type = s7::Type::Byte;
};
struct Sample {
    std::mutex mutex;
    SampleValue latest;
    bool valid = false;
    double raw = 0, period = 1;
    Clock::time_point completed;
    std::string origin, address, unit;
    s7::Type type = s7::Type::Byte;
    bool read(Snapshot &out, Clock::time_point now = Clock::now()) {
        std::lock_guard<std::mutex> lock(mutex);
        if (!valid || latest.acquired <= 0 || !std::isfinite(latest.value) ||
            now - completed > std::chrono::duration<double>(std::max(1.0, period * 3)))
            return false;
        out.latest = latest;
        out.raw = raw;
        out.origin = origin;
        out.address = address;
        out.unit = unit;
        out.type = type;
        return true;
    }
};
class Registry {
  public:
    using Key = std::pair<std::string, std::string>;
    using Map = std::map<Key, std::shared_ptr<Sample>>;
    static Registry &instance() {
        static Registry r;
        return r;
    }
    void replace(Map entries) {
        std::lock_guard<std::mutex> lock(mutex_);
        entries_ = std::move(entries);
    }
    std::shared_ptr<Sample> find(const std::string &device, const std::string &tag) {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = entries_.find({device, tag});
        return it == entries_.end() ? nullptr : it->second;
    }

  private:
    std::mutex mutex_;
    Map entries_;
};
// Bit selection uses the original integer, before engineering scaling.
inline double event_value(const Snapshot &s, int bit) {
    if (bit == -1)
        return s.latest.value;
    const int bits = s.type == s7::Type::Bool ? 1 : s7::width_of(s.type) * 8;
    if (s.type == s7::Type::Real || bit < 0 || bit >= bits)
        throw std::invalid_argument("S7 event bit incompatible with type");
    const auto word = static_cast<uint32_t>(static_cast<int64_t>(s.raw));
    return (word >> bit) & 1u;
}
} // namespace s7_runtime
#endif
