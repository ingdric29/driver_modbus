/*
 * Archivo: command_worker.cpp
 * Descripción: Sondea y ejecuta órdenes locales y externas.
 * Necesita: Configuración SQL, períodos, lote y catálogo de comandos; mutex de comandos.
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
#include "include/driver_metrics.h"
#include "include/command_metrics.h"

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

// Preserve the existing consume/load/execute order; publish diagnostics after releasing
// the catalog lock so file I/O does not extend the critical section used by alarms.
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
    std::unique_lock<std::recursive_mutex> commands(GLOBAL_command_mutex);
    const auto lock_us = elapsed_us(begin);
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
    count = leer_tbcmd(host, getVal(port), schema, user, password);
    const auto catalog_us = elapsed_us(started);
    metrics.record(source, Stage::CatalogSql, catalog_us, count < 0);
    bool failed = count < 0;
    std::vector<CommandResult> results;
    for (int j = 0; j < count; ++j) {
        if (!cmd[j].hab_ejecutar || cmd[j].lote != batch)
            continue;
        CommandResult value;
        value.id = cmd[j].id;
        started = Clock::now();
        value.result = cmd[j].ejecutar();
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
    metrics.record(source, Stage::Batch, batch_us, failed || results.empty());
    commands.unlock();
    const auto prefix = "COMMAND run=" + std::to_string(run) +
                        " execution=" + std::to_string(execution) +
                        " source=" + command_metrics::source_name(source) +
                        " request=" + std::to_string(request_id);
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
        << (failed            ? "failed"
            : results.empty() ? "empty"
                              : "accepted")
        << " mutex_wait_us=" << lock_us << " poll_sql_us=" << poll_us
        << " catalog_sql_us=" << catalog_us << " batch_us=" << batch_us;
    logger_main.registrarError(log.str(), false);
    return result;
}
} // namespace

/**
 * Consume una solicitud local y ejecuta el lote con diagnóstico por comando.
 * Retorno: último resultado Modbus; -1 sin ejecución o ante fallo de consulta.
 * Efectos: Conserva consumo anticipado y catálogo global; un fallo de ack se registra.
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
    double diferencia1 = 0.0;
    double diferencia2 = 0.0;
    PeriodicDeadline metrics_deadline;
    metrics_deadline.defer(60);
    while (driver_running()) {
        if (metrics_deadline.due()) {
            logger_main.registrarError(DriverMetrics::instance().summary(), false);
            logger_main.registrarError(Metrics::instance().summary(), false);
            cout << DriverMetrics::instance().console_summary() << endl;
            metrics_deadline.advance(60);
        }
        diferencia1 = difftime(time(NULL), GLOBAL_ultimo_tiempo_tbejecutar_dblocal);
        if (diferencia1 > GLOBAL_periodo_muestreo_tbejecutar) { // if 1
            GLOBAL_ultimo_tiempo_tbejecutar_dblocal = time(NULL);
            ordenejecucuionlocal();
        }
        diferencia2 = difftime(time(NULL), GLOBAL_ultimo_tiempo_tbejecutar_dbexterna);
        if (diferencia2 > GLOBAL_periodoext_muestreo_tbejecutar) { // if 1
            GLOBAL_ultimo_tiempo_tbejecutar_dbexterna = time(NULL);
            ordenejecucuionexterna();
        }
        esperar(
            std::min(GLOBAL_periodo_muestreo_tbejecutar, GLOBAL_periodoext_muestreo_tbejecutar));
    }
    logger_main.registrarError(Metrics::instance().summary(), false);
    cout << "[ ESTADO ] Lectura de ordenes detenida." << endl;
    return 1;
}
