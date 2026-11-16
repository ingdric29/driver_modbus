// Automonitor - separated from main.cpp; public function contracts unchanged.
// Original implementation: Alejandro Diomede / Automatroni.
#include "include/driver_services.h"
#include "include/conversiones.h"
#include "include/runtime_control.h"
#include <algorithm>
#include <cstdlib>
#include <ctime>
#include <iostream>

using namespace std;

int ordenlecturaconfiguracionciclico() {

    double tbcambio_diferencia_tiempo = 0.0;
    double diferencia_tiempo3 = 0.0;
    double diferencia_tiempo4 = 0.0;
    double diferencia_tiempo5 = 0.0;
    double diferencia_tiempo6 = 0.0;

    while (driver_running()) {
        // orden de lectura de configuracion
        tbcambio_diferencia_tiempo = difftime(time(NULL), GLOBAL_tbcambio_tiempo_ultimochequeo);
        if (tbcambio_diferencia_tiempo >= GLOBAL_periodo_muestreo_tbcambios) {
            GLOBAL_tbcambio_tiempo_ultimochequeo = time(NULL);
            imprimir_mensaje("revision tabla tbcambios", negro);
            if (lee_tbcambios() == 1) {
                GLOBAL_EJECUTARHILOS = false;
                break;
            }
        }

        // borrar registros antiguos locales
        diferencia_tiempo3 = difftime(time(NULL), GLOBAL_ultima_revision_tablas_locales);
        if (diferencia_tiempo3 > GLOBAL_periodo_revision_tablas_locales) {
            GLOBAL_ultima_revision_tablas_locales = time(NULL);
            imprimir_mensaje("orden borrar registros locales", azul);
            ordenborrarregistroslocales();
        }
        // borrar registros antiguos externos
        diferencia_tiempo4 = difftime(time(NULL), GLOBAL_ultima_revision_tablas_externas);
        if (diferencia_tiempo4 > GLOBAL_periodo_revision_tablas_externas) {
            GLOBAL_ultima_revision_tablas_externas = time(NULL);
            imprimir_mensaje("orden borrar registros externos", azul);
            ordenborrarregistrosexternos();
        }

        //sincronizar tablas
        diferencia_tiempo5 = difftime(time(NULL), GLOBAL_sicronizacion_tiempo);
        if (diferencia_tiempo5 > GLOBAL_sicronizacion_periodo) {
            GLOBAL_sicronizacion_tiempo = time(NULL);
            imprimir_mensaje("sincronizar tbvalores", amarillo);
            sincronizar_tablas(&stdblocal, &stdbexterna, "tbvalores",
                               GLOBAL_cantidadregistrossubir);
        }

        //reseteo falla mysql externo
        diferencia_tiempo6 = difftime(time(NULL), GLOBAL_ultimo_reseteo_mysqlexterno);
        if (diferencia_tiempo6 > GLOBAL_periodo_reseteo_mysqlexterno) {
            GLOBAL_ultimo_reseteo_mysqlexterno = time(NULL);
            imprimir_mensaje("se ha activado, la comunicacion con mysql externo", amarillo);
            GLOBAL_ACCESO_EXTERNO = true;
        }
        int tiempoentero = (int)(GLOBAL_periodo_muestreo_tbcambios);
        esperar(tiempoentero);
    }
    imprimir_mensaje("final hilo leer tabla cambios ");
    imprimir_mensaje("fin hilo lectura ordenes de ejecucion ");
    imprimir_mensaje("final hilo revision tablas locales y externas ");
    imprimir_mensaje("[ ESTADO ] Sincronizacion de tablas detenida.");

    return 1;
}

int ordenborrarregistroslocalesciclico() {

    double diferencia_tiempo = 0.0;

    while (driver_running()) {
        diferencia_tiempo = difftime(time(NULL), GLOBAL_ultima_revision_tablas_locales);
        if (diferencia_tiempo > GLOBAL_periodo_revision_tablas_locales) {
            GLOBAL_ultima_revision_tablas_locales = time(NULL);
            ordenborrarregistroslocales();
        }
        int tiempoentero = (int)(GLOBAL_periodo_revision_tablas_locales);
        esperar(tiempoentero);
    }
    imprimir_mensaje("final hilo revision tablas locales ");
    return 1;
}

int ordenborrarregistrosexternosciclico() {

    double diferencia_tiempo = 0.0;

    while (driver_running()) {
        diferencia_tiempo = difftime(time(NULL), GLOBAL_ultima_revision_tablas_externas);
        if (diferencia_tiempo > GLOBAL_periodo_revision_tablas_externas) {
            GLOBAL_ultima_revision_tablas_externas = time(NULL);
            ordenborrarregistrosexternos();
        }
        int tiempoentero = (int)(GLOBAL_periodo_revision_tablas_externas);
        esperar(tiempoentero);
    }
    imprimir_mensaje("final hilo revision tablas externas ");
    return 1;
}

int ordenborrarregistroslocales() {

    //borrar_registros(GLOBAL_maximo_dias_antiguedad_tbinstantaneos, GLOBAL_db_direccion, getVal(GLOBAL_db_puerto), GLOBAL_db_nombre, GLOBAL_db_usuario, GLOBAL_db_clave, "tbinstantaneos");
    borrar_registros(GLOBAL_maximo_dias_antiguedad_tbvalores, GLOBAL_db_direccion,
                     getVal(GLOBAL_db_puerto), GLOBAL_db_nombre, GLOBAL_db_usuario, GLOBAL_db_clave,
                     "tbvalores");
    return 1;
}

int ordenborrarregistrosexternos() {

    //borrar_registros(GLOBAL_maximo_dias_antiguedad_tbinstantaneos_ext, GLOBAL_dbext_direccion, getVal(GLOBAL_dbext_puerto), GLOBAL_dbext_nombre, GLOBAL_dbext_usuario, GLOBAL_dbext_clave, "tbinstantaneos");
    borrar_registros(GLOBAL_maximo_dias_antiguedad_tbvalores_ext, GLOBAL_dbext_direccion,
                     getVal(GLOBAL_dbext_puerto), GLOBAL_dbext_nombre, GLOBAL_dbext_usuario,
                     GLOBAL_dbext_clave, "tbvalores");

    return 1;
}

int orden_sincronizar_tablas_ciclico() {

    double diferencia_tiempo = 0.0;

    while (driver_running()) {
        diferencia_tiempo = difftime(time(NULL), GLOBAL_sicronizacion_tiempo);
        if (diferencia_tiempo > GLOBAL_sicronizacion_periodo) {
            GLOBAL_sicronizacion_tiempo = time(NULL);
            imprimir_mensaje("sincronizar tbvalores. bdlocal=>bdexterna", azul);
            sincronizar_tablas(&stdblocal, &stdbexterna, "tbvalores",
                               GLOBAL_cantidadregistrossubir);
        }
        int tiempoentero = (int)(GLOBAL_sicronizacion_periodo);
        esperar(tiempoentero);
    }
    cout << "[ ESTADO ] Sincronizacion de tablas detenida." << endl;
    return 1;
}
