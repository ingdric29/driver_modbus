/*
 * Archivo: conversiones.cpp
 * Descripción: Convierte números y mantiene helpers de valores heredados.
 * Necesita: Valores y cadenas de entrada; precisión cuando corresponda.
 * Retornos: Número, cadena, bit o predicado según función; el formato decimal puede rechazar
 * precisión inválida.
 */

#include "include/conversiones.h"
#include <cstring>
#include <stdexcept>
#include <iomanip>
#include <locale>

using namespace std;

/**
 * Formatea un double con precisión decimal explícita.
 * Parámetros: double numerodouble, int decimales
 * Retorno: Cadena decimal; lanza invalid_argument si precisión no está entre 0 y 100.
 */
string cast_double_to_string(double numerodouble, int decimales) {
    if (decimales < 0 || decimales > 100)
        throw std::invalid_argument("precision outside [0,100]");
    std::ostringstream out;
    out.imbue(std::locale::classic());
    out << std::fixed << std::setprecision(decimales) << numerodouble;
    return out.str();
}

/**
 * Formatea un float con seis posiciones decimales.
 * Parámetros: float numerofloat
 * Retorno: Cadena; no modifica el valor numérico almacenado en SQL.
 */
string cast_float_to_string(float numerofloat) { return cast_double_to_string(numerofloat, 6); }

/**
 * Convierte un entero a su representación decimal.
 * Parámetros: int numeroint
 * Retorno: Cadena decimal.
 */
string cast_int_to_string(int numeroint) {
    char buffer1[20];
    memset(buffer1, 0, 20);
    sprintf(buffer1, "%i", numeroint);
    string numerostr(buffer1);
    return numerostr;
}

/**
 * Convierte el predicado a una marca textual.
 * Parámetros: bool valor
 * Retorno: "1" para true; "0" para false.
 */
string cast_bool_to_string(bool valor) {

    if (valor) {
        return "1";
    } else {
        return "0";
    }
}

/**
 * Convierte texto a entero mediante la conversión C heredada.
 * Parámetros: string cadena1
 * Retorno: Entero convertido; no valida de forma estricta el texto.
 */
int cast_string_to_int(string cadena1) { // convierte un valor cadena string a entero

    int valor = 0;

    // programa
    valor = atoi(cadena1.c_str());

    return valor;
}

/**
 * Interpreta número o formato heredado con unidades d/h/m/s/k.
 * Parámetros: string cadena1
 * Retorno: Valor float; Alive equivale a 1 y Dead a 0; conserva parsing heredado.
 */
float cast_string_to_float(string cadena1) { // convierte un valor cadena string a float

    float valor = 0.0;
    float dias = 0.0;
    float horas = 0.0;
    float minutos = 0.0;
    float segundos = 0.0;
    float miles = 0.0;

    if (cadena1.compare("Alive") == 0) {
        return 1.0;
    } else if (cadena1.compare("Dead") == 0) {
        return 0.0;
    }

    string cadena2 = "";
    for (std::string::size_type i = 0; i < cadena1.length(); i++) {
        if (cadena1[i] != ',') {
            cadena2 = cadena2 + cadena1[i];
        }
        if (cadena1[i] == 'd') {
            dias = atof(cadena2.c_str()) * 1.0;
            cadena2 = "";
        } else if (cadena1[i] == 'h') {
            horas = atof(cadena2.c_str());
            cadena2 = "";
        } else if (cadena1[i] == 'm') {
            minutos = atof(cadena2.c_str());
            cadena2 = "";
        } else if (cadena1[i] == 's') {
            segundos = atof(cadena2.c_str());
            cadena2 = "";
        } else if (cadena1[i] == 'k') {
            miles = atof(cadena2.c_str());
            cadena2 = "";
        }
    }

    // programa

    if (cadena2.length() == 0) {
        valor = dias + (horas / 24.0) + (minutos / 1440.0) + (segundos / 86400.0) + miles * 1000.0;
    } else {
        valor = atof(cadena2.c_str());
    }

    return valor;
}

/**
 * Convierte texto a número mediante la conversión C heredada.
 * Parámetros: string cadena1
 * Retorno: Número convertido; no valida de forma estricta el texto.
 */
double cast_string_to_double(string cadena1) { // convierte un valor cadena string a float

    double valor = 0.0;
    // programa
    valor = atof(cadena1.c_str());

    return valor;
}

/**
 * Aplica la transformación heredada del factor de potencia.
 * Parámetros: float regVal
 * Retorno: Valor float transformado según el rango de entrada.
 */
float convert_float_fp(float regVal) {
    float PF_Val;

    if (regVal > 1) {
        PF_Val = 2 - regVal;
        //PF is leading
    } else if (regVal < -1) {
        PF_Val = -2 - regVal;
        //PF is leading
    } else if (regVal == 1.0) {
        PF_Val = regVal;
        //PF is at unity
    } else {
        PF_Val = regVal;
        //PF is lagging
    }
    return PF_Val;
}

/**
 * Extrae un bit del entero indicado.
 * Parámetros: int valor, int posicion = 0
 * Retorno: 0 o 1; la posición debe ser válida para el tipo entero.
 */
int obtenervalorbit(int valor, int posicion = 0) {
    int res = 0;
    res = (valor >> posicion) & 0x1;
    return res;
}

/**
 * Convierte cero/no cero a un predicado.
 * Parámetros: int valor
 * Retorno: false para cero; true para otro entero.
 */
bool cast_int_bool(int valor) {
    if (valor == 0) {
        return false;
    } else {
        return true;
    }
}

/**
 * Decide si corresponde persistir según marca y modo solo cambios.
 * Parámetros: bool fueescrito, int solocambios
 * Retorno: false si ya se escribió y solo se registran cambios; true en otro caso.
 */
bool habilitarescribir(bool fueescrito, int solocambios) { return !(fueescrito && solocambios); }

/**
 * Selecciona el menor de los dos valores con la comparación heredada.
 * Parámetros: double v1, double v2
 * Retorno: Uno de los dos valores de entrada.
 */
double funmenor(double v1, double v2) { return (v1 > v2) ? v2 : v1; }

/**
 * Convierte dígitos hexadecimales mayúsculos con el algoritmo heredado.
 * Parámetros: char num[]
 * Retorno: Entero acumulado; no valida estrictamente todos los caracteres.
 */
int converthex2dec(char num[]) {
    int len = strlen(num);
    int base = 1;
    int temp = 0;
    for (int i = len - 1; i >= 0; i--) {
        if (num[i] >= '0' && num[i] <= '9') {
            temp += (num[i] - 48) * base;
            base = base * 16;
        } else if (num[i] >= 'A' && num[i] <= 'F') {
            temp += (num[i] - 55) * base;
            base = base * 16;
        }
    }
    return temp;
}
