/* Contadores por enlace físico; el visor nunca toma el bloqueo de transporte. */
#ifndef AUTOMONITOR_MODBUS_LINK_METRICS_H
#define AUTOMONITOR_MODBUS_LINK_METRICS_H
#include <cstdint>
#include <map>
#include <mutex>
#include <string>
struct ModbusLinkCounters {
    uint64_t connection_attempts = 0, connections = 0, reconnections = 0;
    uint64_t writes = 0, write_failures = 0;
};
class ModbusLinkMetrics {
    std::mutex mutex_;
    std::map<std::string, ModbusLinkCounters> links_;

  public:
    static ModbusLinkMetrics &instance() {
        static ModbusLinkMetrics m;
        return m;
    }
    template <class Update> void update(const std::string &key, Update update) noexcept {
        try {
            std::lock_guard<std::mutex> lock(mutex_);
            update(links_[key]);
        } catch (...) {
        }
    }
    std::map<std::string, ModbusLinkCounters> copy() {
        std::lock_guard<std::mutex> lock(mutex_);
        return links_;
    }
    void clear() {
        std::lock_guard<std::mutex> lock(mutex_);
        links_.clear();
    }
};
#endif
