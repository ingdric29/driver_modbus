/*
 * Archivo: driver_metrics.h
 * Descripción: Acumula contadores y tiempos de lectura/persistencia.
 * Necesita: Resultados de operaciones y contadores atómicos.
 * Retornos: Resúmenes de lecturas y fallos; las omisiones se contabilizan aparte.
 */

#ifndef DRIVER_METRICS_H
#define DRIVER_METRICS_H
#include <atomic>
#include <chrono>
#include <sstream>
#include <cstdint>
struct DriverMetrics {
    std::atomic<uint64_t> reads{0}, read_failures{0}, deferred_reads{0}, maximum_read_us{0};
    std::atomic<uint64_t> connections{0}, reused_connections{0}, stored_samples{0}, sql_failures{0},
        replicated_samples{0}, pending_windows{0};
    static DriverMetrics &instance() {
        static DriverMetrics metrics;
        return metrics;
    }
    void record_read(int result, uint64_t elapsed) {
        if (result == -2 || result == -3) {
            ++deferred_reads;
            return;
        }
        ++reads;
        if (result <= 0)
            ++read_failures;
        auto maximum = maximum_read_us.load();
        while (maximum < elapsed && !maximum_read_us.compare_exchange_weak(maximum, elapsed)) {
        }
    }
    std::string console_summary() const {
        std::ostringstream out;
        out << "[ METRICAS ] Totales desde el inicio\n"
            << "  Llamadas: " << reads.load() << " | Fallos de lectura: " << read_failures.load()
            << " | Pospuestas: " << deferred_reads.load()
            << " | Max. lectura: " << maximum_read_us.load() / 1000.0 << " ms\n"
            << "  Historicos SQL: " << stored_samples.load()
            << " | Fallos SQL: " << sql_failures.load()
            << " | Lotes pendientes: " << pending_windows.load();
        return out.str();
    }
    std::string summary() const {
        std::ostringstream out;
        out << "METRICS reads=" << reads.load() << " read_failures=" << read_failures.load()
            << " deferred_reads=" << deferred_reads.load()
            << " maximum_read_us=" << maximum_read_us.load()
            << " connections=" << connections.load()
            << " reused_connections=" << reused_connections.load()
            << " stored_samples=" << stored_samples.load()
            << " sql_failures=" << sql_failures.load()
            << " replicated_samples=" << replicated_samples.load()
            << " pending_windows=" << pending_windows.load();
        return out.str();
    }
};
class ReadMeasurement {
    int &result_;
    std::chrono::steady_clock::time_point start_ = std::chrono::steady_clock::now();

  public:
    explicit ReadMeasurement(int &result) : result_(result) {}
    ~ReadMeasurement() {
        auto &metrics = DriverMetrics::instance();
        const uint64_t elapsed = std::chrono::duration_cast<std::chrono::microseconds>(
                                     std::chrono::steady_clock::now() - start_)
                                     .count();
        metrics.record_read(result_, elapsed);
    }
};
#endif
