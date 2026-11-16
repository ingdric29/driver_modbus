/*
programa dedarrollado por ing. Alejandro diomede - Automatroni - email: diomede3009@gmail.com
Define la estructura de datos llamada Equipos - tcp y serial
Modificación hecha por: Ing. Javier Flores 12/11/24
*/

#include "include/conversiones.h"
#include <errno.h>
#include <vector>
#include <stack>
#include <ctype.h>
#include "include/cls_defalarmas.h"
#include "include/globales.h"
#include "include/estructuras.h"
#include <sstream>
#include "include/alarm_expression.h"

using namespace std;

extern stdatabase stdblocal;
extern stdatabase stdbexterna;

// metodo constructor
cls_defalarmas::cls_defalarmas() {
    id = 0;
    activa = false;
    reconocida = false;
    expresionactivar = "0";
    expresiondesactivar = "0";
    ejecutado_activar_cmd = false;
    ejecutado_desactivar_cmd = true;
    ejecutado_activar_email = false;
    ejecutado_desactivar_email = true;
    ejecutado_activar_sms = false;
    ejecutado_desactivar_sms = true;
    ejecutado_activar_bd = false;
    ejecutado_desactivar_bd = true;
    ejecutado_activar = false;
    ejecutado_desactivar = true;
    contador_de_acciones_activar = 0;
    contador_de_acciones_desactivar = 0;
    contador_de_acciones_max = 10;
}

// reemplaza los simbolos por valores
string cls_defalarmas::reemplazar_simbolos(const string &expression) {
    std::string output;
    size_t begin = 0;
    while (begin < expression.size()) {
        const size_t marker = expression.find("$(", begin);
        if (marker == std::string::npos) {
            output += expression.substr(begin);
            break;
        }
        output += expression.substr(begin, marker - begin);
        const size_t end = expression.find(')', marker + 2);
        if (end == std::string::npos)
            throw std::invalid_argument("unclosed alarm tag reference");
        const auto reference = expression.substr(marker + 2, end - marker - 2);
        const size_t dot = reference.find('.');
        if (dot == std::string::npos || dot == 0 || dot + 1 == reference.size())
            throw std::invalid_argument("invalid alarm tag reference");
        stres sample{};
        if (leertag2(reference.substr(0, dot), reference.substr(dot + 1), &sample) != 0)
            throw std::invalid_argument("unknown or unavailable alarm tag");
        output += cast_double_to_string(sample.valorfloat);
        begin = end + 1;
    }
    return output;
}

// dado el dispositivo y el tag obtiene el valor

// dado el dispositivo y el tag obtiene el valor.  retorna 0, si todo ok, -1 si hay error
int cls_defalarmas::leertag2(const string &dispositivo, const string &tag, struct stres *local) {

    bool encontrado = false;
    for (int i = 0; i < clscantidad_equipo_mbtcp; i++) {
        if (clsequipo_mbtcp) {
            if ((clsequipo_mbtcp[i].nombre.compare(dispositivo) == 0) &&
                (clsequipo_mbtcp[i].tag.compare(tag) == 0)) {
                std::lock_guard<std::recursive_mutex> state(*clsequipo_mbtcp[i].state_mutex);
                if (!clsequipo_mbtcp[i].resultado_leer)
                    return -1;
                local->valorfloat = clsequipo_mbtcp[i].clsvalorfloat;
                local->valorstr = clsequipo_mbtcp[i].clsvalorstr;
                local->unidad = clsequipo_mbtcp[i].unidad;
                local->indice = i;
                local->puntero_equipo = &clsequipo_mbtcp[0];
                local->time_fechatiempo = clsequipo_mbtcp[i].ultimo_muestreo;
                clsorigen = clsequipo_mbtcp[i].direccion_ip;
                clsregistro = cast_int_to_string(clsequipo_mbtcp[i].registro);
                encontrado = true;
                break;
            }
        }
    }
    if (encontrado == false) {
        for (int i = 0; i < clscantidad_equipo_mbserial; i++) {
            if (clsequipo_mbserial) {
                if ((clsequipo_mbserial[i].nombre.compare(dispositivo) == 0) &&
                    (clsequipo_mbserial[i].tag.compare(tag) == 0)) {
                    std::lock_guard<std::recursive_mutex> state(*clsequipo_mbserial[i].state_mutex);
                    if (!clsequipo_mbserial[i].resultado_leer)
                        return -1;
                    local->valorfloat = clsequipo_mbserial[i].clsvalorfloat;
                    local->valorstr = clsequipo_mbserial[i].clsvalorstr;
                    local->indice = i;
                    local->puntero_equipo = &clsequipo_mbserial[0];
                    local->time_fechatiempo = clsequipo_mbserial[i].ultimo_muestreo;
                    clsorigen = clsequipo_mbserial[i].interface1;
                    clsregistro = cast_int_to_string(clsequipo_mbserial[i].registro);
                    encontrado = true;
                    break;
                }
            }
        }
    }

    if (encontrado == false) {
        for (int i = 0; i < clscantidad_equipo_rs232; i++) {
            if (clsequipo_rs232) {
                if ((clsequipo_rs232[i].nombre.compare(dispositivo) == 0) &&
                    (clsequipo_rs232[i].tag.compare(tag) == 0)) {
                    std::lock_guard<std::recursive_mutex> state(*clsequipo_rs232[i].state_mutex);
                    if (!clsequipo_rs232[i].resultado_leer)
                        return -1;
                    local->valorfloat = clsequipo_rs232[i].clsvalorfloat;
                    local->valorstr = clsequipo_rs232[i].clsvalorstr;
                    local->indice = i;
                    local->puntero_equipo = &clsequipo_rs232[0];
                    local->time_fechatiempo = clsequipo_rs232[i].ultimo_muestreo;
                    clsorigen = clsequipo_rs232[i].interface1;
                    clsregistro = clsequipo_rs232[i].registro;
                    encontrado = true;
                    break;
                }
            }
        }
    }

    if (encontrado) {
        return 0;
    } else {
        return -1;
    }
}

string cls_defalarmas::reordenar_antes_evaluar(string in) { return alarm_postfix(in); }

int cls_defalarmas::evaluar1(string p) { return alarm_evaluate_postfix(p); }

string cls_defalarmas::eliminar_espacios(string p) {
    string pout = "";
    for (size_t i = 0; i < p.size(); i++)
        if (p[i] != ' ')
            pout += p[i];
    return pout;
}

int cls_defalarmas::evaluarcompleto() {
    string expresion_sinespacios1, expresion_sinespacios2;
    string expresion_numerica1, expresion_numerica2;
    string expresion_numerica_ordenada1, expresion_numerica_ordenada2;
    stringstream ss2;
    int resultadoactivar, resultadodesactivar;
    int resultado = 0;
    stres valortag1;
    int res_leertag2 = -1;
    try {

        res_leertag2 = leertag2(clsdispositivo, clstag, &valortag1);

        if ((res_leertag2 == 0) && (valortag1.time_fechatiempo > 0)) {
            // desactivar
            expresion_sinespacios2 = eliminar_espacios(expresiondesactivar);
            ss2.str("");
            ss2 << "expresion desactivar: " << expresion_sinespacios2;
            imprimir_mensaje(ss2.str());
            expresion_numerica2 = reemplazar_simbolos(expresion_sinespacios2);
            expresiondesactivar_numerica = expresion_numerica2;
            ss2.str("");
            ss2 << "expresion desactivar numerica: " << expresiondesactivar_numerica;
            imprimir_mensaje(ss2.str());
            expresion_numerica_ordenada2 = reordenar_antes_evaluar(expresion_numerica2);
            ss2.str("");
            ss2 << "expresion_numerica_ordenada2 " << expresion_numerica_ordenada2;
            imprimir_mensaje(ss2.str());
            resultadodesactivar = evaluar1(expresion_numerica_ordenada2);
            ss2.str("");
            ss2 << "resultado desactivar " << resultadodesactivar;
            imprimir_mensaje(ss2.str());
            //activar
            expresion_sinespacios1 = eliminar_espacios(expresionactivar);
            ss2.str("");
            ss2 << "expresion activar sin espacios " << expresion_sinespacios1;
            imprimir_mensaje(ss2.str());
            expresion_numerica1 = reemplazar_simbolos(expresion_sinespacios1);
            expresionactivar_numerica = expresion_numerica1;
            ss2.str("");
            ss2 << "expresion activar numerica " << expresionactivar_numerica;
            imprimir_mensaje(ss2.str());
            expresion_numerica_ordenada1 = reordenar_antes_evaluar(expresion_numerica1);
            ss2.str("");
            ss2 << "expresion activar numerica ordenada " << expresion_numerica_ordenada1;
            imprimir_mensaje(ss2.str());
            resultadoactivar = evaluar1(expresion_numerica_ordenada1);
            ss2.str("");
            ss2 << "resultado activar " << resultadoactivar;
            imprimir_mensaje(ss2.str());

            if ((resultadoactivar == 0) && (resultadodesactivar == 0)) {
                resultado = 0;
            } else if ((resultadoactivar == 0) && (resultadodesactivar == 1)) {
                resultado = 2;
                activa = false;
                contador_de_acciones_activar = 0;
                insertar_tbhistoalarmas();
                ejecutar_accion(acciondesactivar, activa);

            } else if ((resultadoactivar == 1) && (resultadodesactivar == 0)) {
                resultado = 1;
                activa = true;
                contador_de_acciones_desactivar = 0;
                insertar_tbhistoalarmas();
                ejecutar_accion(accionactivar, activa);

            } else if ((resultadoactivar == 1) && (resultadodesactivar == 1)) {
                resultado = 2;
                activa = false;
                contador_de_acciones_activar = 0;
                insertar_tbhistoalarmas();
                ejecutar_accion(acciondesactivar, activa);
            }

            return resultado;

        } // fin del if
        else {
            return -1;
        }

    } // fin del try

    catch (const std::exception &error) {
        imprimir_mensaje(std::string("Invalid alarm: ") + error.what(), rojo);
        imprimir_mensaje("falla - modulo cls_defalarmas :: evaluarcompleto");
        return -1;
    }
}

// si es 0 todo ok, sino es un error
