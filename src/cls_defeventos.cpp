/*
 * Archivo: cls_defeventos.cpp
 * Descripción: Resuelve tags de eventos y registra cambios de valor.
 * Necesita: Dispositivo, tag, bit opcional, catálogos y perfil SQL local.
 * Retornos: Índice, cantidad cargada o estado de lectura; ver cada método.
 */

#include "include/cls_defeventos.h"
#include "include/globales.h"
#include "include/conversiones.h"

using namespace std;

extern stdatabase stdblocal;
extern stdatabase stdbexterna;

// metodo constructor
/**
 * Inicializa valores, tiempos y referencia del evento.
 * Parámetros: ninguno; utiliza el contexto descrito en el encabezado.
 * Retorno: Sin valor de retorno (constructor).
 */
cls_defeventos::cls_defeventos() {
    valoractual = 0;
    valoranterior = 0;
    tiempoactual = 0;
    tiempoanterior = 0;
    indiceenram = -1;
    pequipo = nullptr;
}

/**
 * Solicita la carga del catálogo de eventos de la base local.
 * Parámetros: ninguno; utiliza el contexto descrito en el encabezado.
 * Retorno: Resultado de actualizar_defeventos: cantidad, 0 o -1 según la operación.
 */
int cls_defeventos::cargar_en_memoriaram() {
    int res;
    res = actualizar_defeventos(&stdblocal);
    return res;
}

// ubica el registro en la tabla de cls_equipos
// de manera de ubicarlo mas rapido
/**
 * Resuelve la referencia del evento contra tags TCP/RTU/RS232.
 * Parámetros: ninguno; utiliza el contexto descrito en el encabezado.
 * Retorno: Índice encontrado o -1.
 * Efectos: Actualiza el puntero y el valor de referencia del evento.
 */
int cls_defeventos::cargar_posicion() {
    indiceenram = -1;
    pequipo = nullptr;
    auto find = [&](cls_equipos *tags, int count, const std::string &protocol) {
        for (int i = 0; i < count; ++i)
            if (tags[i].nombre == nombredispositivo && tags[i].tag == tag) {
                std::lock_guard<std::recursive_mutex> state(*tags[i].state_mutex);
                indiceenram = i;
                pequipo = &tags[i];
                protocolo = protocol;
                valoractual =
                    bit1 >= 0 && bit1 <= 15 ? (tags[i].clsvalor >> bit1) & 1 : tags[i].clsvalor;
                valoranterior = valoractual;
                return true;
            }
        return false;
    };
    if (!find(equipo_mbtcp, GLOBAL_contador_equipos_mbtcp, "MBTCP") &&
        !find(equipo_mbserial, GLOBAL_contador_equipos_mbserial, "MBSERIAL"))
        find(equipo_rs232, GLOBAL_contador_equipos_rs232, "RS232");
    return indiceenram;
}

// lee el valor del tag en ram

/**
 * Detecta y registra cambios del tag o bit asociado.
 * Parámetros: ninguno; utiliza el contexto descrito en el encabezado.
 * Retorno: 1 tras procesar; 0 sin referencia o durante inicialización; -1 si la lectura es
 * inválida.
 * Efectos: El retorno no confirma por sí solo una inserción SQL.
 */
int cls_defeventos::leer_valorenram() {
    double diferencia = 0.0;
    uint16_t x1, x2, x3;
    string campost[8];
    string valorst[8];
    campost[0] = "id_defeventos";
    valorst[0] = cast_int_to_string(id);
    campost[1] = "fechatiempo";
    valorst[1] = cast_time_to_datetime(time(NULL));
    campost[2] = "tiempo";
    valorst[2] = cast_time_to_datetime(time(NULL));
    campost[3] = "concentrador";
    valorst[3] = GLOBAL_nombre_concentrador;
    campost[4] = "nombredispositivo";
    valorst[4] = nombredispositivo;
    campost[5] = "tag";
    valorst[5] = tag;
    campost[6] = "bit1";
    campost[7] = "mensaje";

    if (indiceenram != -1 && pequipo) {
        {
            std::lock_guard<std::recursive_mutex> state(*pequipo->state_mutex);
            if (!pequipo->resultado_leer)
                return -1;
            diferencia = difftime(time(NULL), tiempoanterior);
            tiempoactual = time(NULL);
            if ((bit1 >= 0) && (bit1 <= 15)) {
                valoractual = (pequipo->clsvalor >> bit1) & 0x1;

            } else {
                valoractual = pequipo->clsvalor;
            }
        } // Snapshot complete: SQL below does not hold the device mutex.
        //esta instruccion solo se ejecuta una vez al iniciar programa
        if ((diferencia > 86400.0) && (GLOBAL_estado_primera_ejecucion_programa == 3)) {
            valoranterior = valoractual;
            tiempoanterior = tiempoactual;
            return 0;
        }
        if (valoranterior != valoractual) {
            // aqui van las acciones cuando se detecta cambio de valor
            x1 = (uint16_t)valoranterior;
            x2 = (uint16_t)valoractual;
            x3 = x1 ^ x2;
            if (x3 > 0) {
                if ((x2 == 1) && (bit1 >= 0) && (bit1 <= 15)) {
                    //se activo
                    valorst[6] = cast_int_to_string(bit1);
                    valorst[7] = activado + " - (" + cast_int_to_string(valoractual) + ")";
                } else {
                    //se desactivo
                    valorst[6] = cast_int_to_string(bit1);
                    valorst[7] = desactivado + " - (" + cast_int_to_string(valoractual) + ")";
                }
                //int insertar_valor(struct stdatabase* db, string tabla, string* campo, string* valor, int cantidad)
                insertar_valor(&stdblocal, "tbhistoeventos", &campost[0], &valorst[0], 8);
            }
        }

        valoranterior = valoractual;
        tiempoanterior = tiempoactual;
        return 1;
    } else {
        return 0;
    }
}
