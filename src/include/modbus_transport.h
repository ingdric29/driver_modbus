#ifndef DRIVER_MODBUS_TRANSPORT_H
#define DRIVER_MODBUS_TRANSPORT_H
#include <modbus.h>
#include <map>
#include <mutex>
#include <memory>
#include <string>
#include <functional>
#include <atomic>
#include <cerrno>
#include "retry_backoff.h"
#include "driver_metrics.h"

// Exclusive lease per physical endpoint. Reads and writes use the same registry.
class ModbusLease {
    struct Link {
        std::mutex mutex;
        modbus_t *context = nullptr;
        bool connected = false;
        uint64_t session = 0;
        RetryBackoff retry;
        std::string signature;
        ~Link() {
            if (context) {
                if (connected)
                    modbus_close(context);
                modbus_free(context);
            }
        }
    };
    static std::mutex &registry_mutex() {
        static std::mutex mutex;
        return mutex;
    }
    static std::map<std::string, std::shared_ptr<Link>> &registry() {
        static std::map<std::string, std::shared_ptr<Link>> links;
        return links;
    }
    static std::shared_ptr<Link> find(const std::string &key) {
        std::lock_guard<std::mutex> lock(registry_mutex());
        auto &link = registry()[key];
        if (!link)
            link = std::make_shared<Link>();
        return link;
    }
    std::shared_ptr<Link> link_;
    std::unique_lock<std::mutex> lock_;
    ModbusLease(const std::string &key, const std::string &signature,
                const std::function<modbus_t *()> &create)
        : link_(find(key)), lock_(link_->mutex) {
        if (link_->signature != signature || !link_->context) {
            if (link_->context) {
                if (link_->connected)
                    modbus_close(link_->context);
                modbus_free(link_->context);
            }
            link_->connected = false;
            link_->retry.succeeded();
            link_->context = create();
            link_->signature = signature;
        }
    }

  public:
    ModbusLease(ModbusLease &&) = default;
    static ModbusLease tcp(const std::string &host, int port) {
        const auto key = "tcp:" + host + ":" + std::to_string(port);
        return ModbusLease(key, key, [&] { return modbus_new_tcp(host.c_str(), port); });
    }
    static ModbusLease rtu(const std::string &device, int baud, char parity, int bits, int stops) {
        const auto signature =
            std::to_string(baud) + parity + std::to_string(bits) + ":" + std::to_string(stops);
        return ModbusLease("rtu:" + device, signature, [&] {
            return modbus_new_rtu(device.c_str(), baud, parity, bits, stops);
        });
    }
    uint64_t session() const { return link_->session; }
    modbus_t *get() const { return link_->context; }
    int connect(uint32_t timeout_microseconds) {
        if (!get())
            return -1;
        modbus_set_response_timeout(get(), timeout_microseconds / 1000000,
                                    timeout_microseconds % 1000000);
        if (link_->connected) {
            ++DriverMetrics::instance().reused_connections;
            return 0;
        }
        if (!link_->retry.ready()) {
            errno = EAGAIN;
            return -1;
        }
        ++DriverMetrics::instance().connections;
        link_->connected = modbus_connect(get()) == 0;
        if (link_->connected) {
            static std::atomic<uint64_t> next{0};
            link_->session = ++next;
            link_->retry.succeeded();
        } else
            link_->retry.failed();
        return link_->connected ? 0 : -1;
    }
    void invalidate() {
        if (link_->connected) {
            modbus_close(get());
            link_->retry.failed();
        }
        link_->connected = false;
    }
    // Coordinator calls this only after joining all workers in the generation.
    static void clear() {
        std::lock_guard<std::mutex> lock(registry_mutex());
        registry().clear();
    }
};
#endif
