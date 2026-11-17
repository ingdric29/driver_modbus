/*
 * Archivo: local_acquisition.cpp
 * Descripción: Comparte adquisición S7 entre modo local y catálogo SQL; reutiliza MBTCP.
 * Necesita: Configuración local validada; señales, WorkerGroup, monitor y plazos existentes.
 * Retornos: Código de proceso; publica muestras válidas y calidad de errores en el monitor.
 */
#include "include/local_acquisition.h"
#include "include/s7_batch.h"
#include "include/s7_sql.h"
#include <csignal>
#include "include/worker_group.h"
#include "include/runtime_control.h"
#include "include/monitor_server.h"
#include "include/monitor_state.h"
#include "include/cls_equipos.h"
#include "include/cls_logger.h"
#include "include/modbus_transport.h"
#include "include/globales.h"
#include <iostream>
#include <iomanip>

namespace local_driver {
namespace {
struct TagState {
    Tag config;
    uint64_t key = 0, sequence = 0;
    PeriodicDeadline deadline;
    SampleWindow samples;
    std::unique_ptr<cls_equipos> modbus;
    int timeout_ms = 1000;
    std::shared_ptr<SqlSample> sql_sample;
};
std::atomic<bool> reload_requested{false};
void reload_signal(int) { reload_requested.store(true, std::memory_order_relaxed); }
void publish_s7(TagState &tag, const s7::ReadResult &result, size_t offset) {
    const auto &t = tag.config;
    int rc = result.kind == s7::ErrorKind::Retry ? -3 : -1;
    std::string text, diagnostic;
    time_t acquired = 0;
    SampleValue accepted;
    if (result.ok()) {
        try {
            if (offset > result.bytes.size())
                throw std::invalid_argument("S7 invalid block offset");
            double value = s7::decode(t.address, t.type, result.bytes.data() + offset,
                                      result.bytes.size() - offset, t.scale);
            std::ostringstream out;
            out.imbue(std::locale::classic());
            out << std::setprecision(17) << value;
            text = out.str();
            acquired = std::chrono::system_clock::to_time_t(result.acquired);
            SampleValue sample;
            sample.sequence = ++tag.sequence;
            sample.value = value;
            sample.text = text;
            sample.acquired = acquired;
            tag.samples.observe(sample);
            accepted = sample;
            rc = 1;
        } catch (const std::invalid_argument &error) {
            diagnostic = error.what();
        }
    } else if (result.kind != s7::ErrorKind::Retry) {
        diagnostic =
            "Snap7 rc=" + std::to_string(result.error) + ": " + s7::error_text(result.error);
    }
    if (tag.sql_sample) {
        std::lock_guard<std::mutex> lock(tag.sql_sample->mutex);
        tag.sql_sample->valid = rc > 0;
        if (rc > 0)
            tag.sql_sample->latest = accepted;
    }
    auto &state = monitor::State::instance();
    if (result.kind != s7::ErrorKind::Retry)
        state.detail(tag.key, diagnostic);
    state.publish(tag.key, rc, text, acquired, result.latency_us,
                  result.ok() ? result.completed : monitor::Clock::now());
    DriverMetrics::instance().record_read(rc, result.latency_us);
    tag.deadline.advance(t.period);
}
void poll_s7(const Device &device, s7::Transport &transport,
             const std::vector<std::shared_ptr<TagState>> &tags, s7::TransportStats &previous) {
    std::vector<std::pair<size_t, s7::Address>> due;
    const auto now = PeriodicDeadline::Clock::now();
    for (size_t i = 0; i < tags.size(); ++i)
        if (tags[i]->deadline.due(now))
            due.push_back({i, tags[i]->config.address});
    const int pdu = transport.stats().pdu;
    // Pinned Snap7 byte-read response: S7 header (12), params (2), item header (4).
    const int payload = pdu >= 22 ? std::min(65536, pdu - 18) : 222;
    for (const auto &block : s7::plan(due, payload)) {
        if (!driver_running())
            break;
        auto result = transport.read(block.db, block.start, block.count);
        for (size_t i : block.tags) {
            if (!driver_running())
                break;
            auto &tag = *tags[i];
            if (result.kind == s7::ErrorKind::Remote && block.tags.size() > 1) {
                // A range may contain one unavailable tag: preserve valid neighbors.
                auto individual = transport.read(tag.config.address.db, tag.config.address.byte,
                                                 tag.config.address.width);
                publish_s7(tag, individual, 0);
            } else {
                publish_s7(tag, result, static_cast<size_t>(tag.config.address.byte - block.start));
            }
        }
    }
    const auto stats = transport.stats();
    auto &metrics = DriverMetrics::instance();
    metrics.connections += stats.attempts - previous.attempts;
    metrics.reused_connections +=
        (stats.reads - stats.connections) - (previous.reads - previous.connections);
    previous = stats;
    monitor::S7Link link;
    link.attempts = stats.attempts;
    link.connections = stats.connections;
    link.dbread_calls = stats.reads;
    link.failures = stats.failures;
    link.pdu = stats.pdu;
    monitor::State::instance().s7_link(device.name, link);
}
void acquire(const Device &device, std::vector<std::shared_ptr<TagState>> tags) {
    std::unique_ptr<s7::Transport> transport;
    s7::TransportStats previous;
    if (device.protocol == "SIEMENS_S7")
        transport.reset(new s7::Transport(device.endpoint));
    while (driver_running()) {
        if (transport)
            poll_s7(device, *transport, tags, previous);
        else
            for (auto &tag : tags) {
                if (!driver_running())
                    break;
                if (!tag->deadline.due())
                    continue;
                G_modbus_timeout = static_cast<uint32_t>(tag->timeout_ms) * 1000u;
                tag->modbus->leer();
                tag->deadline.advance(tag->config.period);
            }
        wait_driver_seconds(0.005);
    }
}
} // namespace
void start_sql_s7(WorkerGroup &workers) {
    auto catalog = load_sql_s7();
    auto &state = monitor::State::instance();
    for (const auto &device : catalog.config.devices) {
        std::vector<std::shared_ptr<TagState>> tags;
        for (const auto &entry : catalog.tags) {
            if (entry.device != device.name)
                continue;
            auto tag = std::make_shared<TagState>();
            tag->config = entry.tag;
            tag->sql_sample = entry.sample;
            monitor::Tag info;
            info.device = device.name;
            info.device_key = "s7:" + std::to_string(entry.device_id);
            info.protocol = "SIEMENS_S7";
            info.endpoint = device.endpoint.host + ":" + std::to_string(device.endpoint.port);
            info.source_address = entry.tag.address_text;
            info.name = entry.tag.name;
            info.period = entry.tag.period;
            info.unit = entry.tag.unit;
            info.address = entry.tag.address.byte;
            tag->key = state.add_tag(info);
            tags.push_back(tag);
        }
        workers.start(acquire, device, tags);
    }
    if (!catalog.tags.empty())
        workers.start(write_sql_s7, catalog.tags);
}
int run(const std::string &path, bool check_only) {
    try {
        auto config = load(path);
        if (check_only) {
            std::cout << "[ OK ] Configuracion local valida: " << config.devices.size()
                      << " dispositivos\n";
            return 0;
        }
        auto &state = monitor::State::instance();
        state.configure("local", "", "");
        monitor::Server server(obtenerDirectorio(".runtime"));
        const auto previous_handler = std::signal(SIGHUP, reload_signal);
        struct RestoreSignal {
            decltype(previous_handler) previous;
            ~RestoreSignal() { std::signal(SIGHUP, previous); }
        } restore{previous_handler};
        while (!GLOBAL_stop_requested) {
            reload_requested = false;
            state.begin_catalog();
            GLOBAL_EJECUTARHILOS = true;
            WorkerGroup workers(GLOBAL_EJECUTARHILOS);
            // The legacy TCP reader has shared static scratch fields: retain its
            // single protocol worker while each S7 PLC owns an independent worker.
            std::vector<std::shared_ptr<TagState>> modbus_tags;
            for (const auto &device : config.devices) {
                if (!device.enabled)
                    continue;
                std::vector<std::shared_ptr<TagState>> tags;
                for (const auto &t : device.tags) {
                    auto tag = std::make_shared<TagState>();
                    tag->config = t;
                    tag->timeout_ms = device.endpoint.timeout_ms;
                    monitor::Tag m;
                    m.device_key = device.name;
                    m.device = device.name;
                    m.protocol = device.protocol;
                    m.endpoint = device.endpoint.host + ":" + std::to_string(device.endpoint.port);
                    m.source_address = t.address_text;
                    m.name = t.name;
                    m.unit = t.unit;
                    m.period = t.period;
                    m.slave = device.slave;
                    m.address =
                        device.protocol == "SIEMENS_S7" ? t.address.byte : t.modbus_register;
                    tag->key = state.add_tag(m);
                    if (device.protocol == "MBTCP") {
                        tag->modbus.reset(new cls_equipos());
                        auto &mb = *tag->modbus;
                        mb.nombre = device.name;
                        mb.tag = t.name;
                        mb.protocolo = "MBTCP";
                        mb.habilitar = true;
                        mb.direccion_ip = device.endpoint.host;
                        mb.puerto = device.endpoint.port;
                        mb.direccion_esclavo = device.slave;
                        mb.registro = t.modbus_register;
                        mb.funcion = t.function;
                        mb.interpretacion = t.interpretation;
                        mb.periodo = t.period;
                        mb.escalamiento = static_cast<float>(t.scale);
                        mb.monitor_key = tag->key;
                    }
                    tags.push_back(tag);
                }
                if (device.protocol == "SIEMENS_S7")
                    workers.start(acquire, device, tags);
                else
                    modbus_tags.insert(modbus_tags.end(), tags.begin(), tags.end());
            }
            if (!modbus_tags.empty()) {
                Device lane;
                lane.protocol = "MBTCP";
                workers.start(acquire, lane, modbus_tags);
            }
            state.phase("EJECUTANDO");
            std::cout << "[ OK ] Adquisicion local sin SQL. Monitor: automonitorctl\n"
                      << std::flush;
            std::unique_ptr<Config> candidate;
            while (driver_running()) {
                if (reload_requested.exchange(false)) {
                    try {
                        candidate.reset(new Config(load(path)));
                        state.phase("RECARGANDO");
                        GLOBAL_EJECUTARHILOS = false;
                    } catch (const std::exception &error) {
                        state.event(std::string("WARN: CONFIG: Recarga rechazada: ") +
                                    error.what());
                    }
                }
                wait_driver_seconds(0.02);
            }
            workers.join();
            ModbusLease::clear();
            if (candidate && !GLOBAL_stop_requested)
                config = std::move(*candidate);
            else
                break;
        }
        state.phase("DETENIDO");
        return 0;
    } catch (const std::exception &error) {
        std::cerr << "[ ERROR ] " << error.what() << '\n';
        GLOBAL_EJECUTARHILOS = false;
        ModbusLease::clear();
        return 1;
    }
}
} // namespace local_driver
