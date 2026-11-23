/*
 * Archivo: cls_cmd.h
 * Descripción: Escribe comandos de un registro por Modbus TCP o RTU.
 * Necesita: Destino, esclavo, registro, valor, timeout y estado de ejecución.
 * Retornos: Métodos de escritura: resultado libmodbus; ejecutar: 0 si fue aceptada, negativo
 * ante fallo.
 */

#ifndef CLS_CMD

#define CLS_CMD

#include <stdlib.h>
#include <stdio.h>
#include <iostream>
#include <string.h>
#include <inttypes.h>
#include <time.h>
#include <vector>
#include <errno.h>
#include <modbus.h>

using namespace std;

class cls_cmd {
  public:
    //propiedades

    int id = 0;
    string nombre; //nombre
    string direccion_ip;
    int puerto = 0;
    int direccion_esclavo = 0;
    string interface1;
    int velocidad = 0;
    string paridad;
    int bitsparada = 0;
    string equipo;
    string usuario;
    string password;
    bool habilitar = false;
    string protocolo;

    int registro = 0;
    int clsvalor = 0;
    string datetime;
    bool hab_ejecutar = false;
    string lote;
    string configuration_error;

    // metodos

    // Empty means valid. hab_ejecutar filters manual batches; alarm CMD[id] is explicit.
    string validar() const;
    int ejecutar(void);
    int cmdescribir_serial(void);
    int cmdescribir_tcp(void);
};

#endif
