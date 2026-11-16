// Automonitor - separated from main.cpp; public function contracts unchanged.
// Original implementation: Alejandro Diomede / Automatroni.
#include "include/driver_services.h"
#include "include/conversiones.h"
#include "include/runtime_control.h"
#include <algorithm>
#include <cstdlib>
#include <ctime>
#include <iostream>
#include <iomanip>

using namespace std;

void esperar(double segundos) { wait_driver_seconds(segundos); }

int getVal(string cadena1) { // convierte un valor cadena string a entero

    int valor = 0;

    // programa
    valor = atoi(cadena1.c_str());

    return valor;
}

float getValfloat(string cadena1) { // convierte un valor cadena string a float

    float valor = 0.0;

    // programa
    valor = atof(cadena1.c_str());

    return valor;
}

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
