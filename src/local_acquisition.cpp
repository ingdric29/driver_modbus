/*
 * Archivo: local_acquisition.cpp
 * Descripción: Integra adquisición S7 y lector MBTCP existente sin iniciar SQL.
 * Necesita: Configuración local validada; señales, WorkerGroup, monitor y plazos existentes.
 * Retornos: Código de proceso; publica muestras válidas y calidad de errores en el monitor.
 */
#include "include/local_acquisition.h"
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
};
void acquire(const Device &device, std::vector<std::shared_ptr<TagState>> tags) {
    std::unique_ptr<s7::Transport> transport;
    if (device.protocol == "SIEMENS_S7")
        transport.reset(new s7::Transport(device.endpoint));
    while (driver_running()) {
        for (auto &tag : tags) {
            if (!driver_running())
                break;
            if (!tag->deadline.due())
                continue;
            const auto &t = tag->config;
            if (transport) {
                int rc = -1;
                ReadMeasurement measurement(rc);
                auto result = transport->read(t.address.db, t.address.byte, t.address.width);
                rc = result.kind == s7::ErrorKind::Retry ? -3 : -1;
                std::string text;
                time_t acquired = 0;
                if (result.ok()) {
                    try {
                        double value = s7::decode(t.address, t.type, result.bytes.data(),
                                                  result.bytes.size(), t.scale);
                        std::ostringstream out;
                        out.imbue(std::locale::classic());
                        out << std::setprecision(17) << value;
                        text = out.str();
                        acquired = std::chrono::system_clock::to_time_t(result.acquired);
                        SampleValue sample;
                        sample.sequence = ++tag->sequence;
                        sample.value = value;
                        sample.text = text;
                        sample.acquired = acquired;
                        tag->samples.observe(sample);
                        rc = 1;
                    } catch (const std::invalid_argument &) {
                        rc = -1;
                    }
                }
                monitor::State::instance().publish(tag->key, rc, text, acquired, result.latency_us);
            } else {
                G_modbus_timeout = static_cast<uint32_t>(tag->timeout_ms) * 1000u;
                tag->modbus->leer();
            }
            // Skip missed intervals; never burst to catch up after a slow request.
            tag->deadline.advance(t.period);
        }
        wait_driver_seconds(0.005);
    }
}
} // namespace
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
        state.begin_catalog();
        monitor::Server server(obtenerDirectorio(".runtime"));
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
                m.name = t.name;
                m.unit = t.unit;
                m.period = t.period;
                m.slave = device.slave;
                m.address = device.protocol == "SIEMENS_S7" ? t.address.byte : t.modbus_register;
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
        std::cout << "[ OK ] Adquisicion local sin SQL. Monitor: automonitorctl\n" << std::flush;
        while (driver_running())
            wait_driver_seconds(0.02);
        workers.join();
        state.phase("DETENIDO");
        ModbusLease::clear();
        return 0;
    } catch (const std::exception &error) {
        std::cerr << "[ ERROR ] " << error.what() << '\n';
        GLOBAL_EJECUTARHILOS = false;
        ModbusLease::clear();
        return 1;
    }
}
} // namespace local_driver
