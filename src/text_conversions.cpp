/*
 * Archivo: text_conversions.cpp
 * Descripción: Separa, normaliza y transforma cadenas.
 * Necesita: Cadenas, delimitadores y patrones válidos para la operación solicitada.
 * Retornos: Cadena, vector de cadenas o huella numérica; no modifica argumentos const.
 */

#include "include/conversiones.h"
#include <cstring>
#include <stdexcept>
#include <iomanip>
#include <locale>

using namespace std;

/**
 * Separa la cadena por un carácter usando las reglas heredadas.
 * Parámetros: const std::string &str, char separator
 * Retorno: Vector de fragmentos; puede conservar un elemento final vacío.
 */
StringVector cast_string(const std::string &str, char separator) {
    StringVector result;
    size_t pos1 = 0;
    size_t pos2 = 0;
    while (pos2 != str.npos) {
        pos2 = str.find(separator, pos1);
        if (pos2 != str.npos) {
            if (pos2 > pos1)
                result.push_back(str.substr(pos1, pos2 - pos1));
            pos1 = pos2 + 1;
        }
    }
    result.push_back(str.substr(pos1, str.size() - pos1));
    return result;
}

/**
 * Copia una cadena C terminada en cero.
 * Parámetros: char *arr
 * Retorno: Cadena std::string; arr debe ser no nulo y estar terminado.
 */
string cast_arrchar_to_string(char *arr) {
    string s1;
    s1 = arr;
    return s1;
}

/**
 * Reemplaza un patrón en una copia de la cadena.
 * Parámetros: std::string str, const std::string &from, const std::string &to
 * Retorno: Cadena modificada; from debe ser no vacío.
 */
std::string ReplaceAll(std::string str, const std::string &from, const std::string &to) {
    size_t start_pos = 0;
    while ((start_pos = str.find(from, start_pos)) != std::string::npos) {
        str.replace(start_pos, from.length(), to);
        start_pos += to.length(); // Handles case where 'to' is a substring of 'from'
    }
    return str;
}

/**
 * Divide una cadena con getline y el delimitador indicado.
 * Parámetros: const string &cadena, const char &delim
 * Retorno: Vector de fragmentos según las reglas de getline.
 */
std::vector<string> split1(const string &cadena, const char &delim) {
    std::vector<string> vector2;
    std::stringstream ss1;
    ss1 << cadena;
    std::string token;
    while (std::getline(ss1, token, delim)) {
        vector2.push_back(token);
    }
    return vector2;
}

/**
 * Calcula una suma ponderada heredada de los caracteres.
 * Parámetros: string cadena
 * Retorno: Huella long; no es un hash criptográfico.
 */
long huella(string cadena) {
    int longitud = cadena.length();
    long acum = 0;
    for (int i = 0; i < longitud; i++) {
        acum += cadena[i] * (1 + i);
    }
    return acum;
}

/**
 * Transforma mayúsculas/minúsculas mediante las funciones C heredadas.
 * Parámetros: string str
 * Retorno: Cadena transformada; conserva las limitaciones de caracteres del código actual.
 */
string cast_minus_mayus(string str) {
    string mayus;
    char C;
    for (std::string::size_type i = 0; i < str.length(); i++) {
        C = toupper(str[i]);
        mayus.append(1, C);
    }
    return mayus;
}

/**
 * Transforma mayúsculas/minúsculas mediante las funciones C heredadas.
 * Parámetros: string str
 * Retorno: Cadena transformada; conserva las limitaciones de caracteres del código actual.
 */
string cast_mayus_minus(string str) {
    string minus;
    char C;
    for (std::string::size_type i = 0; i < str.length(); i++) {
        C = tolower(str[i]);
        minus.append(1, C);
    }
    return minus;
}

/**
 * Sustituye secuencias #XX por el carácter hexadecimal correspondiente.
 * Parámetros: const string &entradacadena
 * Retorno: Cadena transformada; requiere secuencias de entrada válidas.
 */
string tratamientocadena(const string &entradacadena) {
    string cadena = "";
    string caracter = "";
    string caracterdec = "";
    char c;
    int numero;
    char numerohex[] = {0, 0, 0};
    string caracter2 = "";
    cadena.append(entradacadena);

    size_t pos = cadena.find("#");
    while (pos != string::npos) {

        caracter = cadena.substr(pos + 1, 2);
        caracter2 = "";
        sprintf(numerohex, "%s", caracter.c_str());
        numero = converthex2dec(numerohex);
        c = char(numero);
        caracter2.push_back(c);
        cadena.replace(pos, 3, caracter2);
        pos = cadena.find("#", pos);
    };

    return cadena;
}

/**
 * Filtra los caracteres cuyo código no supera el espacio.
 * Parámetros: const string &cadenaoriginal
 * Retorno: Cadena con los caracteres conservados.
 */
string limpiarcadena(const string &cadenaoriginal) {
    string cadenanueva;
    int caracter;
    for (std::string::size_type i = 0; i < cadenaoriginal.length(); i++) {
        caracter = (int)cadenaoriginal[i];
        if (caracter > 32) {
            cadenanueva.append(1, cadenaoriginal[i]);
        }
    }
    return cadenanueva;
}
