/*
 * Archivo: equipment_rs232.cpp
 * Descripción: Implementa la lectura específica del protocolo del equipo.
 * Necesita: Campos del equipo y contexto de transporte o buffer serie, según protocolo.
 * Retornos: void; actualiza resultadoleer_rc, valor y estado del equipo.
 */

#include "include/globales.h"
#include "include/conversiones.h"
#include "include/equipment_internal.h"
#include "include/modbus_decode.h"
#include "include/modbus_transport.h"
#include "include/register_batch.h"
#include "include/runtime_control.h"
#include <cerrno>
#include <cmath>
#include <limits>
#include <sstream>

using namespace std;

/**
 * Extrae el valor entre delimitadores del buffer serie.
 * Parámetros: ninguno; utiliza el contexto descrito en el encabezado.
 * Retorno: Ninguno (void); resultadoleer_rc indica éxito o fallo.
 * Efectos: Bloquea el buffer y rechaza una ventana de tamaño negativo.
 */
void cls_equipos::leer_rs232(void) {
    std::lock_guard<std::mutex> serial(*buffer_mutex);

    resultadoleer_rc = -1;
    if (tamanoventana < 0)
        return;
    const size_t largoventana = static_cast<size_t>(tamanoventana);
    string tmpstr = "";
    string token = "";
    size_t pos1, pos2;
    vector<stdelim> vector_stdelim;
    int longitudtoken = 0;
    stdelim estructura1;
    estructura1.tag = tag;
    estructura1.delim1 = html_id;
    estructura1.delim2 = html_final;
    vector_stdelim.push_back(estructura1);
    token.clear();
    rastro = nombre + ";" + interface1 + ";" + tag;
    if (*p_n_buf > 0) // if 1
    {
        buf[*p_n_buf] = 0;
        for (int i = 0; i < *p_n_buf; i++) { // for1
            tmpstr.append((char *)&buf[i], 1);
            if (tmpstr.size() > (largoventana + 1)) {
                tmpstr.erase(0, 1);
            };

            for (stdelim st : vector_stdelim) { //for 2
                pos1 = string::npos;
                pos2 = string::npos;
                pos1 = tmpstr.find(st.delim1);
                if (pos1 != string::npos) { //if3
                    pos2 = tmpstr.find(st.delim2, pos1 + st.delim1.length());
                    if (pos2 != string::npos) { //if4
                        longitudtoken = pos2 - pos1 - st.delim1.length();
                        token = tmpstr.substr(pos1 + st.delim1.length(), longitudtoken);
                        st.valor = token;
                        setvalorstring(limpiarcadena(token));
                        resultadoleer_rc = 1;
                        tmpstr.erase(pos1, st.delim1.length() + longitudtoken);
                    } //if4
                } //if3
            }; // for2
        } // for 1

    } //if1
}
