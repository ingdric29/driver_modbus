/*
 * Archivo: runtime_support.cpp
 * Descripción: Implementa esperas, conversiones heredadas y mensajes operativos.
 * Necesita: Parámetros de cada llamada; señal de parada y mutex de salida.
 * Retornos: Valores convertidos o void; las esperas pueden interrumpirse por parada.
 */

#include "include/driver_services.h"
#include "include/conversiones.h"
#include "include/runtime_control.h"
#include <algorithm>
#include <cstdlib>
#include <ctime>
#include <iostream>
#include <iomanip>

using namespace std;

/**
 * Espera el tiempo solicitado, interrumpible por parada.
 * Parámetros: double segundos
 * Retorno: Ninguno (void).
 * Efectos: Consulta las banderas de ejecución; una duración no positiva se normaliza en el
 * helper.
 */
void esperar(double segundos) { wait_driver_seconds(segundos); }

/**
 * Convierte texto a entero mediante la conversión C heredada.
 * Parámetros: string cadena1
 * Retorno: Entero convertido; no valida de forma estricta el texto.
 */
int getVal(string cadena1) { // convierte un valor cadena string a entero

    int valor = 0;

    // programa
    valor = atoi(cadena1.c_str());

    return valor;
}

/**
 * Convierte texto a número mediante la conversión C heredada.
 * Parámetros: string cadena1
 * Retorno: Número convertido; no valida de forma estricta el texto.
 */
float getValfloat(string cadena1) { // convierte un valor cadena string a float

    float valor = 0.0;

    // programa
    valor = atof(cadena1.c_str());

    return valor;
}

/**
 * Muestra diagnóstico si está habilitado y no repite el mensaje anterior.
 * Parámetros: string str, GLOBAL_colores color
 * Retorno: Ninguno (void).
 * Efectos: Serializa la salida y actualiza el último mensaje incluso si no lo imprime.
 */
void imprimir_mensaje(string str, GLOBAL_colores color) {
    std::lock_guard<std::recursive_mutex> output(GLOBAL_output_mutex);
    //imprimir_mensaje("\033[1;32mse leyeron las configuraciones\033[0m" );
    if (GLOBAL_habilitar_imprimir && (str.compare(GLOBAL_ultimomensajemostrado) != 0)) {
        if (color == nada) {
            cout << "[" << left << cast_time_to_datetime(time(NULL)) << "] - " << str << endl;
        } else {
            cout << "\033[1;" << color << "m" << "[" << left << cast_time_to_datetime(time(NULL))
                 << "] - " << str << "\033[0m" << endl;
        }
    }
    GLOBAL_ultimomensajemostrado = str;
}

/**
 * Muestra el mensaje salvo que repita el anterior.
 * Parámetros: string str, GLOBAL_colores color
 * Retorno: Ninguno (void).
 * Efectos: Serializa la salida; no depende de la bandera de diagnóstico.
 */
void imprimir_mensaje_importante(string str, GLOBAL_colores color) {
    std::lock_guard<std::recursive_mutex> output(GLOBAL_output_mutex);
    //imprimir_mensaje("\033[1;32mse leyeron las configuraciones\033[0m" );
    if (str.compare(GLOBAL_ultimomensajemostrado) != 0) {
        if (color == nada) {
            cout << "[" << left << cast_time_to_datetime(time(NULL)) << "] - " << str << endl;
        } else {
            cout << "\033[1;" << color << "m" << "[" << left << cast_time_to_datetime(time(NULL))
                 << "] - " << str << "\033[0m" << endl;
        }
    }
    GLOBAL_ultimomensajemostrado = str;
}
