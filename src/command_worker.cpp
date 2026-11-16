// Automonitor - separated from main.cpp; public function contracts unchanged.
// Original implementation: Alejandro Diomede / Automatroni.
#include "include/driver_services.h"
#include "include/conversiones.h"
#include "include/runtime_control.h"
#include <algorithm>
#include <cstdlib>
#include <ctime>
#include <iostream>
#include "include/driver_metrics.h"

using namespace std;

int ordenejecucuionlocal() {
    std::lock_guard<std::recursive_mutex> commands(GLOBAL_command_mutex);
    int res_cmd_local = 0;
    time_t res_segundos;
    int res_ejecutar = -1;
    string res_lote = "";
    //orden de ejecucion local
    if (leer_tbejecutar(res_cmd_local, res_segundos, res_lote, GLOBAL_db_direccion,
                        getVal(GLOBAL_db_puerto), GLOBAL_db_nombre, GLOBAL_db_usuario,
                        GLOBAL_db_clave) < 0)
        return -1;
    imprimir_mensaje("cmd local result = " + cast_int_to_string(res_cmd_local), amarillo);
    if (res_cmd_local == 1) {
        GLOBAL_contador_comandos = leer_tbcmd(GLOBAL_db_direccion, getVal(GLOBAL_db_puerto),
                                              GLOBAL_db_nombre, GLOBAL_db_usuario, GLOBAL_db_clave);
        if (GLOBAL_contador_comandos > 0) {

            for (int j = 0; j < GLOBAL_contador_comandos; j++) {
                if (cmd[j].hab_ejecutar == true) {
                    if (cmd[j].lote.compare(res_lote) == 0) {
                        res_ejecutar = cmd[j].ejecutar();

                        if (res_ejecutar == 0) {
                            escribir_tbcmd_fechahora(cmd[j].id, GLOBAL_db_direccion,
                                                     getVal(GLOBAL_db_puerto), GLOBAL_db_nombre,
                                                     GLOBAL_db_usuario, GLOBAL_db_clave);
                        }
                    }
                }
            }; // fin del for
        };
    };

    return res_ejecutar;
}

int ordenejecucuionexterna() {
    std::lock_guard<std::recursive_mutex> commands(GLOBAL_command_mutex);
    int res_cmd_externa = 0;
    time_t res_segundos;
    int res_ejecutar_externa = -1;
    string res_lote = "";
    // orden de jecucion externa desde una base de datos en la nube
    if (leer_tbejecutar(res_cmd_externa, res_segundos, res_lote, GLOBAL_dbext_direccion,
                        getVal(GLOBAL_dbext_puerto), GLOBAL_dbext_nombre, GLOBAL_dbext_usuario,
                        GLOBAL_dbext_clave) < 0)
        return -1;
    imprimir_mensaje("cmd externo result = " + cast_int_to_string(res_cmd_externa), amarillo);
    if (res_cmd_externa == 1) {
        GLOBAL_contadorext_comandos =
            leer_tbcmd(GLOBAL_dbext_direccion, getVal(GLOBAL_dbext_puerto), GLOBAL_dbext_nombre,
                       GLOBAL_dbext_usuario, GLOBAL_dbext_clave);
        if (GLOBAL_contadorext_comandos > 0) {

            for (int j = 0; j < GLOBAL_contadorext_comandos; j++) {
                if (cmd[j].hab_ejecutar == true) {
                    if (cmd[j].lote.compare(res_lote) == 0) {
                        res_ejecutar_externa = cmd[j].ejecutar();
                        if (res_ejecutar_externa == 0) {
                            escribir_tbcmd_fechahora(
                                cmd[j].id, GLOBAL_dbext_direccion, getVal(GLOBAL_dbext_puerto),
                                GLOBAL_dbext_nombre, GLOBAL_dbext_usuario, GLOBAL_dbext_clave);
                        }
                    }
                }
            }; // fin del ford
        };
    };
    return res_ejecutar_externa;
}

int ordenejecucionciclico() {
    double diferencia1 = 0.0;
    double diferencia2 = 0.0;
    PeriodicDeadline metrics_deadline;
    metrics_deadline.defer(60);
    while (driver_running()) {
        if (metrics_deadline.due()) {
            logger_main.registrarError(DriverMetrics::instance().summary(), false);
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
    cout << "[ ESTADO ] Lectura de ordenes detenida." << endl;
    return 1;
}
