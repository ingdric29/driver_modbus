/*
 * Archivo: command_worker.cpp
 * Descripción: Sondea y ejecuta órdenes locales y externas.
 * Necesita: Configuración SQL, períodos, lote y listas privadas por dispositivo.
 * Retornos: Resultado de ejecución de la orden o 1 al terminar el ciclo; ver cada función.
 */

#include "include/driver_services.h"
#include "include/conversiones.h"
#include "include/runtime_control.h"
#include <algorithm>
#include <cstdlib>
#include <ctime>
#include <iostream>
#include <vector>
#include <map>
#include "include/driver_metrics.h"
#include "include/command_metrics.h"
#include "include/command_poll.h"
#include "include/command_repository.h"

using namespace std;

namespace {
using command_metrics::Clock;
using command_metrics::elapsed_us;
using command_metrics::Metrics;
using command_metrics::Source;
using command_metrics::Stage;

struct CommandResult {
    int id = 0, result = -1, ack = -1;
    uint64_t execute_us = 0, ack_us = 0;
    command_metrics::TransportTrace transport;
};

// A batch owns its commands. SQL, PLC and logging do not lock the alarm catalog.
int execute_commands(bool external) {
    const auto source = external ? Source::External : Source::Local;
    const auto &host = external ? GLOBAL_dbext_direccion : GLOBAL_db_direccion;
    const auto &port = external ? GLOBAL_dbext_puerto : GLOBAL_db_puerto;
    const auto &schema = external ? GLOBAL_dbext_nombre : GLOBAL_db_nombre;
    const auto &user = external ? GLOBAL_dbext_usuario : GLOBAL_db_usuario;
    const auto &password = external ? GLOBAL_dbext_clave : GLOBAL_db_clave;
    auto &count = external ? GLOBAL_contadorext_comandos : GLOBAL_contador_comandos;
    auto &metrics = Metrics::instance();
    const auto begin = Clock::now();
    const uint64_t lock_us = 0; // Private batch: no global command/catalog lock.
    metrics.record(source, Stage::MutexWait, lock_us);
    int requested = 0, request_id = 0, result = -1;
    time_t seconds = 0;
    string batch;
    auto started = Clock::now();
    const auto polled = leer_tbejecutar(requested, seconds, batch, host, getVal(port), schema, user,
                                        password, &request_id);
    const auto poll_us = elapsed_us(started);
    metrics.record(source, Stage::PollSql, poll_us, polled < 0);
    if (polled < 0)
        return -1;
    imprimir_mensaje(string(external ? "cmd externo result = " : "cmd local result = ") +
                         cast_int_to_string(requested),
                     amarillo);
    if (requested != 1)
        return -1;

    // Process-local correlation only. Durable identities/states belong to stage 5.
    static const auto run = std::chrono::duration_cast<std::chrono::microseconds>(
                                std::chrono::system_clock::now().time_since_epoch())
                                .count();
    const auto execution = metrics.next_execution();
    started = Clock::now();
    std::vector<cls_cmd> batch_commands;
    count = command_repository::load_batch(host, getVal(port), schema, user, password, batch,
                                           batch_commands);
    const auto catalog_us = elapsed_us(started);
    metrics.record(source, Stage::CatalogSql, catalog_us, count < 0);
    bool failed = count < 0;
    started = Clock::now();
    struct DeviceGroup {
        bool disabled = false;
        string reason;
    };
    std::map<string, DeviceGroup> groups;
    std::map<int, string> ids;
    for (const auto &command : batch_commands) {
        auto &group = groups[command.nombre];
        group.disabled = group.disabled || !command.habilitar;
    }
    for (const auto &command : batch_commands) {
        auto &group = groups[command.nombre];
        if (group.disabled)
            continue;
        const auto reason = command.validar();
        if (group.reason.empty() && !reason.empty())
            group.reason = "comando " + std::to_string(command.id) + ": " + reason;
        const auto previous = ids.emplace(command.id, command.nombre);
        if (!previous.second) {
            group.reason = "id de comando duplicado en el lote";
            groups[previous.first->second].reason = group.reason;
        }
    }
    std::vector<std::pair<int, string>> invalid;
    size_t skipped = 0;
    for (const auto &command : batch_commands) {
        const auto &group = groups.at(command.nombre);
        if (group.disabled)
            ++skipped;
        else if (!group.reason.empty())
            invalid.emplace_back(command.id, group.reason);
    }
    const auto validation_us = elapsed_us(started);
    if (count > 0)
        metrics.record(source, Stage::Validation, validation_us, !invalid.empty());
    failed = failed || !invalid.empty();
    std::vector<CommandResult> results;
    // Validate by device, but transmit in the original global ID order.
    for (int j = 0; j < count; ++j) {
        const auto &group = groups.at(batch_commands[j].nombre);
        if (group.disabled || !group.reason.empty())
            continue;
        CommandResult value;
        value.id = batch_commands[j].id;
        started = Clock::now();
        value.result = batch_commands[j].ejecutar();
        value.execute_us = elapsed_us(started);
        value.transport = command_metrics::transport_trace();
        result = value.result;
        metrics.record(source, Stage::Execute, value.execute_us, result != 0);
        if (result == 0) {
            started = Clock::now();
            value.ack =
                escribir_tbcmd_fechahora(value.id, host, getVal(port), schema, user, password);
            value.ack_us = elapsed_us(started);
            metrics.record(source, Stage::AckSql, value.ack_us, value.ack < 0);
        }
        failed = failed || result != 0 || value.ack < 0;
        results.push_back(value);
    }
    const auto batch_us = elapsed_us(begin);
    metrics.record(source, Stage::Batch, batch_us, failed || (results.empty() && !skipped));
    const auto prefix = "COMMAND run=" + std::to_string(run) +
                        " execution=" + std::to_string(execution) +
                        " source=" + command_metrics::source_name(source) +
                        " request=" + std::to_string(request_id);
    for (const auto &command : batch_commands) {
        if (groups.at(command.nombre).disabled)
            logger_main.registrarError(
                prefix + " command=" + std::to_string(command.id) +
                    " device=" + command_metrics::field(command.nombre) +
                    " validation=skipped reason=\"dispositivo deshabilitado\"",
                false);
    }
    for (const auto &entry : invalid) {
        logger_main.registrarError(
            prefix + " command=" + std::to_string(entry.first) +
                " validation=rejected reason=" + command_metrics::field(entry.second),
            false);
    }
    for (const auto &value : results) {
        std::ostringstream log;
        log << prefix << " command=" << value.id
            << " modbus=" << (value.result == 0 ? "accepted" : "failed") << " ack="
            << (value.result != 0 ? "skipped"
                : value.ack == 0  ? "ok"
                                  : "failed")
            << " attempts=" << value.transport.attempts << " error=" << value.transport.error
            << " execute_us=" << value.execute_us
            << " link_wait_us=" << value.transport.link_wait_us
            << " connect_us=" << value.transport.connect_us
            << " wire_us=" << value.transport.wire_us << " ack_us=" << value.ack_us;
        logger_main.registrarError(log.str(), false);
    }
    std::ostringstream log;
    log << prefix << " batch=" << command_metrics::field(batch) << " loaded=" << count
        << " matched=" << results.size() << " status="
        << (!invalid.empty()  ? (results.empty() ? "rejected" : "partial")
            : failed          ? "failed"
            : results.empty() ? (skipped ? "skipped" : "empty")
                              : "accepted")
        << " skipped=" << skipped << " rejected=" << invalid.size() << " mutex_wait_us=" << lock_us
        << " poll_sql_us=" << poll_us << " catalog_sql_us=" << catalog_us
        << " validation_us=" << validation_us << " batch_us=" << batch_us;
    logger_main.registrarError(log.str(), false);
    return failed ? -1 : result;
}
} // namespace

/**
 * Consume una solicitud local y ejecuta el lote con diagnóstico por comando.
 * Retorno: 0 si las escrituras y confirmaciones terminaron bien; -1 sin ejecución o con errores.
 * Efectos: Conserva consumo anticipado; lote privado, catálogo de alarmas independiente.
 */
int ordenejecucuionlocal() { return execute_commands(false); }

/**
 * Consume una solicitud externa habilitada y ejecuta el lote con diagnóstico.
 * Retorno/efectos: iguales al recorrido local; no cambia la política de reintentos.
 */
int ordenejecucuionexterna() { return execute_commands(true); }

/**
 * Ejecuta la tarea indicada respetando su planificación y estado de parada.
 * Parámetros: ninguno; utiliza el contexto descrito en el encabezado.
 * Retorno: 1 al finalizar; no equivale a confirmar todas las operaciones subordinadas.
 */
int ordenejecucionciclico() {
    CommandPollSchedule schedule(GLOBAL_periodo_muestreo_tbejecutar,
                                 GLOBAL_periodoext_muestreo_tbejecutar);
    PeriodicDeadline metrics_deadline;
    metrics_deadline.defer(60);
    while (driver_running()) {
        if (metrics_deadline.due()) {
            logger_main.registrarError(DriverMetrics::instance().summary(), false);
            logger_main.registrarError(Metrics::instance().summary(), false);
            cout << DriverMetrics::instance().console_summary() << endl;
            metrics_deadline.advance(60);
        }
        if (schedule.local_due()) {
            Metrics::instance().record(Source::Local, Stage::PollLag,
                                       elapsed_us(schedule.next_local()));
            // Wall timestamps remain available for legacy diagnostics only.
            GLOBAL_ultimo_tiempo_tbejecutar_dblocal = time(NULL);
            ordenejecucuionlocal();
            schedule.local_finished();
        }
        const bool external_enabled = GLOBAL_habilitar_acceso_externo != 0;
        if (driver_running() && schedule.external_due(external_enabled)) {
            Metrics::instance().record(Source::External, Stage::PollLag,
                                       elapsed_us(schedule.next_external()));
            GLOBAL_ultimo_tiempo_tbejecutar_dbexterna = time(NULL);
            ordenejecucuionexterna();
            schedule.external_finished();
        }
        wait_driver_until(std::min(schedule.next(external_enabled), metrics_deadline.next()));
    }
    logger_main.registrarError(Metrics::instance().summary(), false);
    cout << "[ ESTADO ] Lectura de ordenes detenida." << endl;
    return 1;
}
