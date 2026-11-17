/*
 * Archivo: s7_transport.h
 * Descripción: Posee una sesión Snap7 de solo lectura, con exclusión y reconexión.
 * Necesita: Endpoint IPv4, DB/rango válido y timeouts de 50..5000 ms.
 * Retornos: ReadResult sin datos ante error; constructor rechaza configuración inválida.
 */
#ifndef AUTOMONITOR_S7_TRANSPORT_H
#define AUTOMONITOR_S7_TRANSPORT_H
#include <chrono>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace s7 {
struct Endpoint {
    std::string host;
    int port = 102, rack = 0, slot = 1, timeout_ms = 1000;
};
void validate_endpoint(const Endpoint &endpoint);
enum class ErrorKind { None, Retry, Transport, Remote, Configuration };
struct ReadResult {
    ErrorKind kind = ErrorKind::None;
    int error = 0;
    std::vector<uint8_t> bytes;
    uint64_t latency_us = 0;
    std::chrono::system_clock::time_point acquired;
    bool ok() const { return kind == ErrorKind::None; }
};
struct TransportStats {
    uint64_t attempts = 0, connections = 0, reads = 0, failures = 0;
    int pdu = 0;
};
class Transport {
    struct Impl;
    std::unique_ptr<Impl> impl_;

  public:
    explicit Transport(const Endpoint &endpoint);
    ~Transport();
    Transport(const Transport &) = delete;
    Transport &operator=(const Transport &) = delete;
    ReadResult read(int db, int start, int bytes);
    TransportStats stats() const;
    void disconnect();
};
} // namespace s7
#endif
