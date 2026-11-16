// Automonitor - separated from main.cpp; public function contracts unchanged.
// Original implementation: Alejandro Diomede / Automatroni.
#include "include/driver_services.h"
#include "include/conversiones.h"
#include "include/runtime_control.h"
#include <algorithm>
#include <cstdlib>
#include <ctime>
#include <iostream>
#include "include/cls_defalarmas.h"
#include "include/monitor_state.h"

using namespace std;

int ordenrevisarposiblesalarmasciclico() {
    while (driver_running()) {
        double diferencia_tiempoalarmas = 0.0;
        diferencia_tiempoalarmas = difftime(time(NULL), GLOBAL_ultima_revision_alarmas);
        if (diferencia_tiempoalarmas >= GLOBAL_periodo_revision_alarmas) {
            ordenrevisarposiblesalarmas();
        }
        esperar(GLOBAL_periodo_revision_alarmas);
    }
    cout << "[ ESTADO ] Revision de alarmas detenida." << endl;
    return 1;
}

int ordenrevisarposiblesalarmas() {

    imprimir_mensaje("inicio revision tabla de alarmas: " +
                         cast_int_to_string(GLOBAL_contador_defalarmas),
                     negro);
    for (int j = 0; j < GLOBAL_contador_defalarmas; j++) {
        int result = defalarmas[j].evaluarcompleto();
        monitor::State::instance().alarm(defalarmas[j].id, result >= 0, defalarmas[j].activa);
    }
    imprimir_mensaje("terminada revision tabla de alarmas", negro);
    GLOBAL_ultima_revision_alarmas = time(NULL);
    return 1;
}
