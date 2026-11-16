/*
 * Archivo: alarm_history.cpp
 * Descripción: Registra activación o desactivación de alarmas en SQL.
 * Necesita: Alarma, muestra asociada, concentrador y tabla tbhistoalarmas de la base local.
 * Retornos: 0 si confirma una inserción; -1 si no se inserta o falla la operación.
 */

#include "include/driver_services.h"
#include "include/cls_defalarmas.h"
#include "include/conversiones.h"
#include <sstream>
#include <vector>

using namespace std;

/**
 * Inserta el histórico de activación/desactivación con su dispositivo.
 * Parámetros: ninguno; utiliza el contexto descrito en el encabezado.
 * Retorno: 0 si confirma inserción; -1 si no inserta o falla la escritura.
 */
int cls_defalarmas::insertar_tbhistoalarmas() {
    string arr_campos1[13];
    string arr_valores1[13];
    int res_insertardblocal = 0;
    int res_leertag2;
    stres valortag;
    arr_campos1[0] = "id";
    arr_campos1[1] = "id_alarma";
    arr_campos1[2] = "fechatiempo";
    arr_campos1[3] = "tiempo";
    arr_campos1[4] = "tipo";
    arr_campos1[5] = "descripcion";
    arr_campos1[6] = "valor";
    arr_campos1[7] = "accion";
    arr_campos1[8] = "condicion";
    arr_campos1[9] = "concentrador";
    arr_campos1[10] = "dispositivo";
    arr_campos1[11] = "tag";
    arr_campos1[12] = "reconocida";
    valortag.valorstr = "0";
    if (activa) {
        ejecutado_desactivar_bd = false;

        if (ejecutado_activar_bd == false) {
            fechaocurrencia_activacion = time(NULL);
            arr_valores1[0] = "NULL";
            arr_valores1[1] = cast_int_to_string(id);
            arr_valores1[2] = cast_time_to_datetime(fechaocurrencia_activacion);
            arr_valores1[3] = cast_time_to_datetime(time(NULL));
            arr_valores1[4] = "activacion";
            arr_valores1[5] = descripcion;
            res_leertag2 = leertag2(clsdispositivo, clstag, &valortag);
            if ((res_leertag2 == 0) && (valortag.time_fechatiempo > 0)) {
                arr_valores1[6] = cast_double_to_string(valortag.valorfloat); // valortag.valorstr
            } else {
                return -1;
            }
            arr_valores1[7] = accionactivar;
            arr_valores1[8] = expresionactivar + " [" + expresionactivar_numerica + "]";
            arr_valores1[9] = GLOBAL_nombre_concentrador;
            arr_valores1[10] = clsdispositivo;
            arr_valores1[11] = clstag;
            arr_valores1[12] = "0";

            res_insertardblocal =
                insertar_valor(&stdblocal, "tbhistoalarmas", &arr_campos1[0], &arr_valores1[0], 13);

            if (res_insertardblocal == 0) {
                ejecutado_activar_bd = true;
                return 0;
            }
        }
    } else {
        ejecutado_activar_bd = false;
        if (ejecutado_desactivar_bd == false) {
            fechaocurrencia_desactivacion = time(NULL);
            arr_valores1[0] = "NULL";
            arr_valores1[1] = cast_int_to_string(id);
            arr_valores1[2] = cast_time_to_datetime(fechaocurrencia_desactivacion);
            arr_valores1[3] = cast_time_to_datetime(time(NULL));
            arr_valores1[4] = "desactivacion";
            arr_valores1[5] = descripcion;
            res_leertag2 = leertag2(clsdispositivo, clstag, &valortag);
            if ((res_leertag2 == 0) && (valortag.time_fechatiempo > 0)) {
                arr_valores1[6] = cast_double_to_string(valortag.valorfloat); // valortag.valorstr
            } else {
                return -1;
            }
            arr_valores1[6] = cast_double_to_string(valortag.valorfloat);
            arr_valores1[7] = acciondesactivar;
            arr_valores1[8] = expresiondesactivar + " [" + expresiondesactivar_numerica + "]";
            arr_valores1[9] = GLOBAL_nombre_concentrador;
            arr_valores1[10] = clsdispositivo;
            arr_valores1[11] = clstag;
            arr_valores1[12] = "0";

            res_insertardblocal =
                insertar_valor(&stdblocal, "tbhistoalarmas", &arr_campos1[0], &arr_valores1[0], 13);

            if (res_insertardblocal == 0) {
                ejecutado_desactivar_bd = true;
                return 0;
            }
        }
    }

    return -1;
}
