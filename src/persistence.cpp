#include "include/driver_services.h"
#include "include/runtime_control.h"
#include "include/driver_metrics.h"
#include "include/conversiones.h"
#include <algorithm>
#include <map>
#include "include/pending_batch.h"
#include "include/sql_session.h"
namespace {
struct HistoricalWrite {
    PendingBatch pending;
    std::string tag, name, trace;
    int id = 0;
    uint64_t revision = 0;
    std::weak_ptr<std::recursive_mutex> generation;
    PeriodicDeadline retry;
};
// Only the writer thread touches this map. It outlives configuration workers
// so an acknowledged prefix or pending suffix survives a reload.
std::map<std::string, HistoricalWrite> historical_pending;
std::string sample_key(const cls_equipos& tag) {
    return tag.protocolo + "\x1f" + tag.nombre + "\x1f" + tag.tag + "\x1f" + std::to_string(tag.id);
}
void flush_historical() {
    for (auto it = historical_pending.begin(); it != historical_pending.end() && !GLOBAL_stop_requested;) {
        auto& write = it->second;
        if (!write.retry.due()) { ++it; continue; }
        const bool stored = write.pending.persist([&](const std::vector<SampleValue>& samples) {
            try {
                auto connection = sql_connection("tcp://" + GLOBAL_db_direccion + ":" + GLOBAL_db_puerto,
                    GLOBAL_db_usuario, GLOBAL_db_clave, GLOBAL_db_nombre);
                store_sample_batch(connection, "tbvalores", samples, write.tag, write.id, write.trace,
                    GLOBAL_nombre_concentrador, write.name);
                return true;
            } catch (const std::exception& error) {
                sql_sessions().invalidate();
                ++DriverMetrics::instance().sql_failures;
                logger_main.registrarError(std::string("Historical transaction failed: ") + error.what());
                return false;
            }
        });
        if (!stored) { write.retry.defer(1); ++it; continue; }
        auto confirm = [&](cls_equipos* tags, int count) {
            for (int i = 0; i < count; ++i) {
                if (sample_key(tags[i]) != it->first || tags[i].state_mutex != write.generation.lock()) continue;
                std::lock_guard<std::recursive_mutex> state(*tags[i].state_mutex);
                tags[i].fueescritodb = tags[i].change_revision == write.revision;
                tags[i].ultimo_insertar = time(nullptr);
                tags[i].history_deadline.advance(std::max(tags[i].periodo, GLOBAL_periodo_minimo_escritura_tbvalores));
            }
        };
        confirm(equipo_mbtcp, GLOBAL_contador_equipos_mbtcp);
        confirm(equipo_mbserial, GLOBAL_contador_equipos_mbserial);
        confirm(equipo_rs232, GLOBAL_contador_equipos_rs232);
        it = historical_pending.erase(it);
        DriverMetrics::instance().pending_windows = historical_pending.size();
    }
}
}

int ordenescrituraequipos(cls_equipos* equipo, int arge_cantidad) {
    int result = 0;
    for (int i = 0; i < arge_cantidad; ++i) {
        auto& tag = equipo[i];
        SampleValue sample;
        std::string name, tag_name, trace;
        int id, errors, total_errors;
        bool online, instant_due, status_due;
        {
            std::lock_guard<std::recursive_mutex> state(*tag.state_mutex);
            const auto now = PeriodicDeadline::Clock::now();
            sample = tag.latest_sample;
            name = tag.nombre; tag_name = tag.tag; trace = tag.rastro; id = tag.id;
            errors = *tag.pcontador_errores; total_errors = *tag.pcontador_errores_acumulados;
            online = *tag.pestadocomunicacion;
            instant_due = tag.resultado_leer && tag.instant_deadline.due(now);
            status_due = tag.status_deadline.due(now);
            const auto key = sample_key(tag);
            if (tag.resultado_leer && tag.registrable && tag.history_deadline.due(now)
                && habilitarescribir(tag.fueescritodb, tag.solocambios)
                && !tag.sample_window.empty() && historical_pending.count(key) == 0) {
                HistoricalWrite write;
                write.pending = PendingBatch(tag.sample_window.samples(tag.preservar_picos));
                write.tag = tag.tag; write.name = tag.nombre; write.trace = tag.rastro;
                write.id = tag.id; write.revision = tag.change_revision;
                write.generation = tag.state_mutex;
                historical_pending.emplace(key, std::move(write));
                DriverMetrics::instance().pending_windows = historical_pending.size();
                tag.sample_window.clear();
            }
        }
        // Database I/O operates exclusively on immutable copies. Acquisition
        // never waits for SQL while trying to publish a sample.
        if (status_due) {
            string fields[] = {"nombre", "errores", "errores_acumulados", "estado", "fechatiempo", "concentrador"};
            string values[] = {name, std::to_string(errors), std::to_string(total_errors), online ? "1" : "0",
                cast_time_to_datetime(time(nullptr)), GLOBAL_nombre_concentrador};
            const int stored = insertar_inteligente(&stdblocal, "tbdispositivos_sta", 0, fields, values, 6);
            std::lock_guard<std::recursive_mutex> state(*tag.state_mutex);
            if (stored == 0) tag.status_deadline.advance(GLOBAL_periodo_minimo_escritura_tbdispositivos_sta);
            else { tag.status_deadline.defer(1); result = -1; }
        }
        if (instant_due) {
            string fields[] = {"timedate", "fechatiempo", "tag", "valorstr", "valorfloat", "dispositivo", "rastro", "concentrador", "nombredispositivo"};
            string values[] = {cast_time_to_datetime(time(nullptr)), cast_time_to_datetime(sample.acquired), tag_name,
                sample.text, cast_double_to_string(sample.value), std::to_string(id), trace, GLOBAL_nombre_concentrador, name};
            const int stored = insertar_inteligente3(&stdblocal, "tbinstantaneos", 2, 7, 8, fields, values, 9);
            std::lock_guard<std::recursive_mutex> state(*tag.state_mutex);
            if (stored == 0) {
                tag.ultima_escritura_tbinstantaneo = time(nullptr);
                tag.instant_deadline.advance(tag.periodo);
            } else { tag.instant_deadline.defer(1); result = -1; }
        }
    }
    return result;
}

// Correccion: Habilitacion y deshabilitacion de la escritura en la base de datos cuando se pierde la comunicacion
// Fecha de correccion: 30/10/24
int ordenescrituraequiposciclico() {
	int res_ordenlecturaequipos;
	bool iniciado = false;
	double menor;
	//cout << "GLOBAL_EJECUTARHILOS: " << GLOBAL_EJECUTARHILOS << endl;
	while (driver_running()) {
		//cout << "escrituras de tags " << endl;
		{ // Each tag carries its own communication status.

			if (GLOBAL_contador_equipos_mbtcp > 0) {
				ordenescrituraequipos(equipo_mbtcp, GLOBAL_contador_equipos_mbtcp);
			}
			if (GLOBAL_contador_equipos_mbserial > 0) {
				ordenescrituraequipos(equipo_mbserial, GLOBAL_contador_equipos_mbserial);
			}

			//cout << "GLOBAL_contador_equipos_rs232: " << GLOBAL_contador_equipos_rs232 << endl;
			if (GLOBAL_contador_equipos_rs232 > 0) {
				ordenescrituraequipos(equipo_rs232, GLOBAL_contador_equipos_rs232);
			}
		}

		flush_historical();
		menor = funmenor(GLOBAL_periodo_minimo_escritura_tbvalores, GLOBAL_periodo_minimo_escritura_tbdispositivos_sta);
//		cout << "esperar: " << menor << endl;
		auto wake = PeriodicDeadline::Clock::now() + std::chrono::milliseconds(200);
        auto inspect = [&](cls_equipos* tags, int count) {
            for (int i = 0; i < count; ++i) {
                std::lock_guard<std::recursive_mutex> state(*tags[i].state_mutex);
                if (!tags[i].resultado_leer) continue;
                wake = std::min(wake, tags[i].instant_deadline.next());
                if (tags[i].registrable && habilitarescribir(tags[i].fueescritodb, tags[i].solocambios)
                    && historical_pending.count(sample_key(tags[i])) == 0)
                    wake = std::min(wake, tags[i].history_deadline.next());
            }
        };
        inspect(equipo_mbtcp, GLOBAL_contador_equipos_mbtcp);
        inspect(equipo_mbserial, GLOBAL_contador_equipos_mbserial);
        inspect(equipo_rs232, GLOBAL_contador_equipos_rs232);
        wait_driver_until(std::max(wake, PeriodicDeadline::Clock::now() + std::chrono::milliseconds(1)));
		iniciado = true;
	}
	if (iniciado) cout << " fin hilo escritura en base de datos local" << endl;
	return 1;
}
