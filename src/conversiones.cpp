// Original implementation: Alejandro Diomede / Automatroni; maintenance: Javier Flores.
// Function bodies and platform alternatives preserved during modularization.
#include "include/conversiones.h"
#include <cstring>
#include <stdexcept>
#include <iomanip>
#include <locale>

using namespace std;

string cast_double_to_string(double numerodouble, int decimales) {
    if (decimales < 0 || decimales > 100)
        throw std::invalid_argument("precision outside [0,100]");
    std::ostringstream out;
    out.imbue(std::locale::classic());
    out << std::fixed << std::setprecision(decimales) << numerodouble;
    return out.str();
}

string cast_float_to_string(float numerofloat) { return cast_double_to_string(numerofloat, 6); }

string cast_int_to_string(int numeroint) {
    char buffer1[20];
    memset(buffer1, 0, 20);
    sprintf(buffer1, "%i", numeroint);
    string numerostr(buffer1);
    return numerostr;
}

string cast_bool_to_string(bool valor) {

    if (valor) {
        return "1";
    } else {
        return "0";
    }
}

int cast_string_to_int(string cadena1) { // convierte un valor cadena string a entero

    int valor = 0;

    // programa
    valor = atoi(cadena1.c_str());

    return valor;
}

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

double cast_string_to_double(string cadena1) { // convierte un valor cadena string a float

    double valor = 0.0;
    // programa
    valor = atof(cadena1.c_str());

    return valor;
}

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

int obtenervalorbit(int valor, int posicion = 0) {
    int res = 0;
    res = (valor >> posicion) & 0x1;
    return res;
}

bool cast_int_bool(int valor) {
    if (valor == 0) {
        return false;
    } else {
        return true;
    }
}

bool habilitarescribir(bool fueescrito, int solocambios) { return !(fueescrito && solocambios); }

double funmenor(double v1, double v2) { return (v1 > v2) ? v2 : v1; }

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
