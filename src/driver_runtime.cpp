/*
 * Archivo: driver_runtime.cpp
 * Descripción: Coordina carga, recarga y ciclo de vida de los trabajadores.
 * Necesita: Configuración global válida, catálogos SQL y servicios de adquisición/monitor.
 * Retornos: muestrear: 0 al terminar o -1 ante fallo; carga de catálogo: 1 o -1.
 */

#include "include/driver_services.h"
#include "include/conversiones.h"
#include "include/runtime_control.h"
#include <algorithm>
#include <cstdlib>
#include <ctime>
#include <iostream>
#include "include/worker_group.h"
#include "include/s7_sql.h"
#include "include/modbus_transport.h"
#include "include/driver_metrics.h"
#include "include/monitor_state.h"
#include "include/startup_console.h"
#include "include/cls_defalarmas.h"

using namespace std;

/**
 * Coordina generaciones de catálogo y el arranque/unión de trabajadores.
 * Parámetros: ninguno; utiliza el contexto descrito en el encabezado.
 * Retorno: 0 al terminar; -1 si falla la carga del catálogo.
 * Efectos: Los recursos se reemplazan solo tras unir los hilos; otras excepciones se propagan.
 */
int muestrear(void) {
    GLOBAL_estado_primera_ejecucion_programa = 1;
    cout << "[ INICIO ] Cargando catalogo de dispositivos..." << endl;
    while (!GLOBAL_stop_requested) {
        monitor::State::instance().phase("CARGANDO");
        if (ordenlecturaconfiguracion() < 0) {
            liberarmemoria();
            return -1;
        }
        if (GLOBAL_stop_requested) {
            liberarmemoria();
            break;
        }
        cout << "\n  ADQUISICION\n";
        startup_row("Tags Modbus TCP", std::to_string(GLOBAL_contador_equipos_mbtcp));
        startup_row("Tags Modbus RTU", std::to_string(GLOBAL_contador_equipos_mbserial));
        startup_row("Tags RS232", std::to_string(GLOBAL_contador_equipos_rs232));
        startup_row("Puertos RS232", std::to_string(GLOBAL_contador_equipos_rs232_distintos));
        cout << endl;
        GLOBAL_EJECUTARHILOS = true;
        actualizar_valor_condicion(&stdblocal, "tbdispositivos_sta", "estado", "0", "`id`>'0'");
        monitor::State::instance().phase("EJECUTANDO");
        WorkerGroup workers(GLOBAL_EJECUTARHILOS);
        local_driver::start_sql_s7(workers);
        workers.start(ordenlecturaconfiguracionciclico);
        workers.start(ordenejecucionciclico);
        workers.start(ordenlecturaequiposciclico, equipo_mbtcp, GLOBAL_contador_equipos_mbtcp);
        workers.start(ordenlecturaequiposciclico, equipo_mbserial,
                      GLOBAL_contador_equipos_mbserial);
        workers.start(ordenlecturaequiposciclico, equipo_rs232, GLOBAL_contador_equipos_rs232);
        workers.start(ordenrevisarposiblesalarmasciclico);
        workers.start(orden_actualizarvalores_clsdefeventos);
        workers.start(ordenescrituraequiposciclico);
        GLOBAL_estado_primera_ejecucion_programa = 2;
        for (int serial = 0; serial < GLOBAL_contador_equipos_rs232_distintos; ++serial)
            workers.start(lectura_puerto_serial, &equipo_rs232_distintos[serial]);
        cout << "[ OK ] Trabajadores iniciados.\n"
             << "       Monitor: automonitorctl  |  Detener: Ctrl+C\n"
             << endl;
        workers.join();
        monitor::State::instance().phase(GLOBAL_stop_requested ? "DETENIENDO" : "RECARGANDO");
        ModbusLease::clear();

        cout << "\n[ ESTADO ] Trabajadores detenidos." << endl;

        esperar(5);
        liberarmemoria();
    }
    logger_main.registrarError(DriverMetrics::instance().summary(), false);
    cout << DriverMetrics::instance().console_summary() << endl;
    return 0;
}

/**
 * Carga los catálogos y registra tags/alarmas en el monitor.
 * Parámetros: ninguno; utiliza el contexto descrito en el encabezado.
 * Retorno: 1 si termina la carga; -1 ante fallo o conflicto de puerto.
 */
int ordenlecturaconfiguracion() {
    // Coordinator only: no workers exist while definitions are replaced.
    string campo2[5];
    lee_tbcambios();
    {
        if (actualizar_leer_equipo(MBTCP) < 0)
            return -1;
        if (actualizar_leer_equipo(MBSERIAL) < 0)
            return -1;
        if (actualizar_leer_equipo(RS232) < 0)
            return -1;
        if (leer_tbcmd(GLOBAL_db_direccion, getVal(GLOBAL_db_puerto), GLOBAL_db_nombre,
                       GLOBAL_db_usuario, GLOBAL_db_clave) < 0)
            return -1;
        if (actualizar_directorio(&stdblocal) < 0)
            return -1;
        if (actualizar_smslist(&stdblocal) < 0)
            return -1;
        if (actualizar_defalarmas(&stdblocal) < 0)
            return -1;
        if (actualizar_defeventos(&stdblocal) < 0)
            return -1;
        for (int rtu = 0; rtu < GLOBAL_contador_equipos_mbserial; ++rtu) {
            for (int serial = 0; serial < GLOBAL_contador_equipos_rs232; ++serial) {
                if (equipo_mbserial[rtu].interface1 == equipo_rs232[serial].interface1) {
                    logger_main.registrarError(
                        "Modbus RTU y RS232 directo no pueden compartir el mismo puerto serie");
                    return -1;
                }
            }
        }
        actualizar_posiciones_clsdefeventos();

        //campo2[0] = "id";
        //campo2[1] = "activar";
        //campo2[2] = "desactivar";
        //campo2[3] = "descripcion";
        //replicaciontablaselectiva(&stdblocal, "tbdefalarmas", &stdblocal, "tbalarmas", &campo2[0], 4, 256);

        campo2[0] = "id";
        campo2[1] = "nombre";
        campo2[2] = "";
        campo2[3] = "";
        replicaciontablaselectiva(&stdblocal, "tbdispositivos", &stdblocal, "tbdispositivos_sta",
                                  &campo2[0], 2, 256);

        actualizar_valor_condicion(&stdblocal, "tbdispositivos_sta", "concentrador",
                                   GLOBAL_nombre_concentrador, "id >= 0");

        auto &monitor_state = monitor::State::instance();
        monitor_state.begin_catalog();
        auto register_tags = [&](cls_equipos *tags, int count) {
            for (int i = 0; i < count; ++i) {
                auto &t = tags[i];
                monitor::Tag info;
                info.device = t.nombre;
                info.protocol = t.protocolo;
                info.endpoint = t.protocolo == "MBTCP"
                                    ? t.direccion_ip + ":" + std::to_string(t.puerto)
                                    : t.interface1;
                info.slave = t.direccion_esclavo;
                // Length prefixes prevent ambiguous names/endpoints from merging devices.
                for (const auto &part :
                     {info.protocol, info.device, info.endpoint, std::to_string(info.slave)})
                    info.device_key += std::to_string(part.size()) + ":" + part;
                info.name = t.tag;
                info.unit = t.unidad;
                info.address = t.registro;
                info.period = std::isfinite(t.periodo) && t.periodo > 0 ? t.periodo : 1;
                t.monitor_key = monitor_state.add_tag(info);
            }
        };
        register_tags(equipo_mbtcp, GLOBAL_contador_equipos_mbtcp);
        register_tags(equipo_mbserial, GLOBAL_contador_equipos_mbserial);
        register_tags(equipo_rs232, GLOBAL_contador_equipos_rs232);
        for (int i = 0; i < GLOBAL_contador_defalarmas; ++i)
            monitor_state.add_alarm(defalarmas[i].id, defalarmas[i].clsdispositivo,
                                    defalarmas[i].descripcion);
        imprimir_mensaje("se leyeron las configuraciones", verde);
    }
    return 1;
}
