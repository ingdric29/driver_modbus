// Automonitor - separated from main.cpp; public function contracts unchanged.
// Original implementation: Alejandro Diomede / Automatroni.
#include "include/driver_services.h"
#include "include/conversiones.h"
#include "include/runtime_control.h"
#include <algorithm>
#include <cstdlib>
#include <ctime>
#include <iostream>
#include "include/cls_defeventos.h"

using namespace std;

int actualizar_posiciones_clsdefeventos() {

    if (GLOBAL_contador_defeventos == -1) {
        actualizar_defeventos(&stdblocal);
    }
    if (GLOBAL_contador_defeventos == -1)
        return 0;
    int ind = 0;
    while (ind < GLOBAL_contador_defeventos) {
        defeventos[ind].cargar_posicion();
        ind++;
    }

    return 0;
}

int actualizarvalores_clsdefeventos() {
    int ind = 0;
    if (GLOBAL_estado_primera_ejecucion_programa < 3) {
        return -1;
    }
    if (GLOBAL_contador_defeventos < 0)
        return -1;
    while (ind < GLOBAL_contador_defeventos) {
        defeventos[ind].leer_valorenram();
        ind++;
    }
    return 0;
}

int orden_actualizarvalores_clsdefeventos() {

    double diferencia_tiempo = 0.0;

    while (driver_running()) {
        diferencia_tiempo = difftime(time(NULL), GLOBAL_tiempo_ultima_revision_eventos);
        if (diferencia_tiempo > GLOBAL_periodo_revision_eventos) {
            GLOBAL_tiempo_ultima_revision_eventos = time(NULL);
            imprimir_mensaje("iniado revision tabla de eventos en ram", azul);
            actualizarvalores_clsdefeventos();
            imprimir_mensaje("terminada revision tabla de eventos en ram", azul);
        }
        int tiempoentero = (int)(GLOBAL_periodo_revision_eventos);
        esperar(tiempoentero);
    }
    cout << "[ ESTADO ] Revision de eventos detenida." << endl;
    return 1;
}
