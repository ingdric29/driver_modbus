/*
 * Archivo: event_worker.cpp
 * Descripción: Actualiza referencias y valores de eventos del catálogo.
 * Necesita: Catálogos de equipos/eventos y estado de adquisición.
 * Retornos: Códigos de actualización o 1 al terminar el trabajador; ver cada función.
 */

#include "include/driver_services.h"
#include "include/conversiones.h"
#include "include/runtime_control.h"
#include <algorithm>
#include <cstdlib>
#include <ctime>
#include <iostream>
#include "include/cls_defeventos.h"

using namespace std;

/**
 * Resuelve las referencias de los eventos contra el catálogo vigente.
 * Parámetros: ninguno; utiliza el contexto descrito en el encabezado.
 * Retorno: 0, incluido el caso sin catálogo disponible.
 */
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

/**
 * Actualiza los eventos cuando la adquisición ya publicó valores.
 * Parámetros: ninguno; utiliza el contexto descrito en el encabezado.
 * Retorno: 0 tras recorrerlos; -1 si aún no hay estado o catálogo disponible.
 */
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

/**
 * Ejecuta la tarea indicada respetando su planificación y estado de parada.
 * Parámetros: ninguno; utiliza el contexto descrito en el encabezado.
 * Retorno: 1 al finalizar; no equivale a confirmar todas las operaciones subordinadas.
 */
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
