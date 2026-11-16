#include "include/driver_services.h"
#include "include/runtime_control.h"
#include "include/driver_metrics.h"
#include "include/conversiones.h"
#include <algorithm>
#include <map>
#include "include/register_batch.h"
namespace {
int requested_registers(const cls_equipos& tag) {
    if (tag.interpretacion == "longle64") return 4;
    if (tag.interpretacion.compare(0, 7, "float32") == 0 || tag.interpretacion == "longle" || tag.interpretacion == "longbe") return 2;
    return std::max(1, std::min(tag.cantidad1, 20));
}
void prefetch_due_tags(cls_equipos* tags, int count) {
    std::map<std::string, std::vector<int>> due;
    const auto now = PeriodicDeadline::Clock::now();
    for (int i = 0; i < count; ++i) {
        auto& tag = tags[i];
        std::lock_guard<std::recursive_mutex> state(*tag.state_mutex);
        if (!tag.read_deadline.due(now) || *tag.pcontador_errores > tag.cuenta_errores_maximo
            || (tag.protocolo != "MBTCP" && tag.protocolo != "MBSERIAL")
            || (tag.funcion != "hr" && tag.funcion != "rr")) continue;
        std::string endpoint = tag.protocolo == "MBTCP" ? tag.direccion_ip + ":" + std::to_string(tag.puerto)
            : tag.interface1 + ":" + std::to_string(tag.velocidad) + ":" + tag.paridad + ":" + std::to_string(tag.numerodebits) + ":" + std::to_string(tag.bitsparada);
        due[endpoint + ":" + std::to_string(tag.direccion_esclavo) + ":" + tag.funcion].push_back(i);
    }
    for (const auto& group : due) {
        if (group.second.size() < 2) continue;
        const auto& tag = tags[group.second.front()];
        std::vector<RegisterRange> ranges;
        for (int i : group.second) ranges.push_back({tags[i].registro, requested_registers(tags[i])});
        // Only optimize if merging actually reduces the number of transactions.
        if (merge_register_ranges(ranges).size() >= ranges.size()) continue;
        auto lease = tag.protocolo == "MBTCP" ? ModbusLease::tcp(tag.direccion_ip, tag.puerto)
            : ModbusLease::rtu(tag.interface1, tag.velocidad, tag.paridad.empty() ? 'N' : tag.paridad[0], tag.numerodebits, tag.bitsparada);
        if (lease.connect(G_modbus_timeout) == 0) prefetch_registers(lease, tag.direccion_esclavo, tag.funcion == "hr" ? 3 : 4, ranges);
    }
}
}

int ordenlecturaequipos(cls_equipos* equipo, int arge_cantidad) {
    RegisterCycle cycle;
    prefetch_due_tags(equipo, arge_cantidad);
	/* ORDENES DE LECTURAS DE LOS DISPOSITIVOS SEGUN TABLECOM*/
	int resultado=-1;
	try {
		for (int i = 0; i < arge_cantidad; i++) {
            std::lock_guard<std::recursive_mutex> state(*equipo[i].state_mutex); // for 1

			const auto now = PeriodicDeadline::Clock::now();
			resultado = -1;
			if (equipo[i].read_deadline.due(now)) {
				//cout << "*";
				resultado = equipo[i].leer();
                equipo[i].read_deadline.advance(equipo[i].periodo);

			} // end if 1 */

		} // end for
		GLOBAL_estado_primera_ejecucion_programa = 3;
		return resultado;
	}
	catch (...) {
		imprimir_mensaje("falla ordenlecturaequipos");

		std::string mensajeError = std::string("DEBUG: ordenlecturaequipos fail!") +
                               " | File: " + __FILE__ +
                               " | Function: " + __FUNCTION__ +
                               " | Line: " + std::to_string(__LINE__);

        logger_main.registrarError(mensajeError);
		return -1;
	}
}
//Correccion: Se habilita y deshabilita la escritura en la base de datos en base a un error de comunicacion.
//Fecha de Correccion: 30/10/24
int ordenlecturaequiposciclico(cls_equipos* equipo, int arge_cantidad) {
	bool iniciado = false;

	try
	{
		while (driver_running()) {
			if (arge_cantidad == 0)
			{

				break;
			};
			ordenlecturaequipos(equipo, arge_cantidad);
			iniciado = true;

			auto wake = PeriodicDeadline::Clock::now() + std::chrono::milliseconds(200);
            for (int tag = 0; tag < arge_cantidad; ++tag) {
                std::lock_guard<std::recursive_mutex> state(*equipo[tag].state_mutex);
                wake = std::min(wake, equipo[tag].read_deadline.next());
            }
            wait_driver_until(wake);
		}
		if (iniciado){
		cout << " fin hilo lectura variables protocolo " << equipo[0].protocolo << endl;};
		return 1;
	}
	catch (...){

		cout << "falla en ordenlecturaequiposciclico" << endl;

		std::string mensajeError = std::string("DEBUG: ordenlecturaequiposciclico fail!")  +
                               " | File: " + __FILE__ +
                               " | Function: " + __FUNCTION__ +
                               " | Line: " + std::to_string(__LINE__);

        logger_main.registrarError(mensajeError);
		return 2;
	}

}
