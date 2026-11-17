/*
 * Archivo: s7_transport.cpp
 * Descripción: Lee DB por Snap7 sin exponer operaciones de escritura.
 * Necesita: Endpoint validado; reloj monotónico y biblioteca compartida Snap7.
 * Retornos: Lectura completa o error tipificado; no publica buffers parciales.
 */
#include "include/s7_transport.h"
#include "include/retry_backoff.h"
#include "../third_party/snap7/release/wrappers/c-cpp/snap7.h"
#include <arpa/inet.h>
#include <mutex>
#include <stdexcept>

namespace s7 {
void validate_endpoint(const Endpoint &e) {
    in_addr address;
    if (inet_pton(AF_INET, e.host.c_str(), &address) != 1 || e.port < 1 || e.port > 65535 ||
        e.rack < 0 || e.rack > 7 || e.slot < 0 || e.slot > 31 || e.timeout_ms < 50 ||
        e.timeout_ms > 5000)
        throw std::invalid_argument("S7 invalid IPv4, port, rack/slot or timeout");
}
struct Transport::Impl {
    Endpoint endpoint;
    TS7Client client;
    mutable std::mutex mutex;
    RetryBackoff retry;
    TransportStats stats;
    explicit Impl(const Endpoint &e) : endpoint(e) {
        word port = static_cast<word>(e.port);
        int timeout = e.timeout_ms;
        if (client.SetParam(p_u16_RemotePort, &port) ||
            client.SetParam(p_i32_PingTimeout, &timeout) ||
            client.SetParam(p_i32_SendTimeout, &timeout) ||
            client.SetParam(p_i32_RecvTimeout, &timeout))
            throw std::runtime_error("S7 failed to set communication parameters");
    }
};
Transport::Transport(const Endpoint &e) {
    validate_endpoint(e);
    impl_.reset(new Impl(e));
}
Transport::~Transport() = default;
TransportStats Transport::stats() const {
    std::lock_guard<std::mutex> lock(impl_->mutex);
    return impl_->stats;
}
void Transport::disconnect() {
    std::lock_guard<std::mutex> lock(impl_->mutex);
    impl_->client.Disconnect();
    impl_->retry.succeeded();
}
ReadResult Transport::read(int db, int start, int count) {
    std::lock_guard<std::mutex> lock(impl_->mutex);
    ReadResult result;
    const auto began = std::chrono::steady_clock::now();
    auto &p = *impl_;
    if (db < 1 || db > 65535 || start < 0 || count < 1 || count > 65536 ||
        start > 2097152 - count) {
        result.kind = ErrorKind::Configuration;
        result.error = errCliInvalidParams;
    } else if (!p.client.Connected() && !p.retry.ready()) {
        result.kind = ErrorKind::Retry;
    } else {
        if (!p.client.Connected()) {
            ++p.stats.attempts;
            result.error =
                p.client.ConnectTo(p.endpoint.host.c_str(), p.endpoint.rack, p.endpoint.slot);
            if (result.error) {
                result.kind = ErrorKind::Transport;
                p.client.Disconnect();
                p.retry.failed();
            } else {
                ++p.stats.connections;
                p.stats.pdu = p.client.PDULength();
            }
        }
        if (result.ok()) {
            result.bytes.resize(count);
            ++p.stats.reads;
            // Snap7 splits byte reads using the negotiated PDU and its headers.
            result.error = p.client.DBRead(db, start, count, result.bytes.data());
            if (result.error) {
                const auto code = static_cast<unsigned>(result.error);
                const bool transport = (code & 0x000fffffu) || code == errCliJobTimeout ||
                                       code == errCliInvalidPlcAnswer ||
                                       code == errCliInvalidDataSizeRecvd;
                result.kind = transport ? ErrorKind::Transport : ErrorKind::Remote;
                result.bytes.clear();
                if (transport) {
                    p.client.Disconnect();
                    p.retry.failed();
                }
            } else {
                result.acquired = std::chrono::system_clock::now();
                p.retry.succeeded();
            }
        }
    }
    if (!result.ok() && result.kind != ErrorKind::Retry)
        ++p.stats.failures;
    result.latency_us = std::chrono::duration_cast<std::chrono::microseconds>(
                            std::chrono::steady_clock::now() - began)
                            .count();
    return result;
}
} // namespace s7
