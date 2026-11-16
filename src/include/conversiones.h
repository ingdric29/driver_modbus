/*
 * Archivo: conversiones.h
 * Descripción: Convierte números y mantiene helpers de valores heredados.
 * Necesita: Valores y cadenas de entrada; precisión cuando corresponda.
 * Retornos: Número, cadena, bit o predicado según función; el formato decimal puede rechazar
 * precisión inválida.
 */

#ifndef conversiones
#define conversiones
#ifdef _WIN32
#include <winsock2.h>
#else
#include <byteswap.h>
#include <arpa/inet.h>

#include <netdb.h>
#include <unistd.h>
#include <sys/fcntl.h>
#include <sys/errno.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <net/if.h>
#endif
#ifdef _WIN32
#pragma comment(lib, "ws2_32.lib")
#endif
#include <inttypes.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <iostream>
#include <string.h>
#include <stdlib.h>
#include <vector>
#include <sstream>

//#include <winsock2.h>
//#pragma comment(lib, "ws2_32.lib")

// byteswap.h
// la libreria arpa/inet es para sistemas linux
// en windows debe quitarse y usar winsock2.h

using namespace std;

typedef std::vector<std::string> StringVector;

/* Get a float from 4 bytes (Modbus) without any conversion (ABCD) */
float cast_get_float_abcd(const uint16_t *src);

/* Get a float from 4 bytes (Modbus) in inversed format (DCBA) */
float cast_get_float_dcba(const uint16_t *src);

/* Get a float from 4 bytes (Modbus) with swapped bytes (BADC) */
float cast_get_float_badc(const uint16_t *src);

/* Get a float from 4 bytes (Modbus) with swapped words (CDAB) */
float cast_get_float_cdab(const uint16_t *src);

time_t cast_datetime_to_time(const std::string &);

string cast_time_to_datetime(time_t);

int cast_string_to_int(string);

float cast_string_to_float(string);

double cast_string_to_double(string);

string cast_float_to_string(float);

string cast_int_to_string(int);

//string cast_double_to_string(double);

string cast_double_to_string(double numerodouble, int decimales = 6);

StringVector cast_string(const std::string &str, char separator);

string cast_arrchar_to_string(char *arr);

string cast_bool_to_string(bool valor);

std::string ReplaceAll(std::string str, const std::string &from, const std::string &to);

std::vector<string> split1(const string &cadena, const char &delim);

float convert_float_fp(float regVal);

int obtenervalorbit(int valor, int posicion);

long huella(string cadena);

string cast_minus_mayus(string str);

string cast_mayus_minus(string str);

bool cast_int_bool(int valor);

bool habilitarescribir(bool fuecambiado, int solocambios);

int converthex2dec(char num[]);

string tratamientocadena(const string &entradacadena);

string limpiarcadena(const string &cadenaoriginal);

string fecha_para_emails();
string fecha_para_emails_local();
string fecha_para_emails_local_es();
string fecha_para_emails_utc_es();

int listainterfaces();
unsigned long long valormac_from_ifrname(char *if_name);
int imprimirmac_from_ifrname(char *if_name);
#endif
