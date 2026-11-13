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
#include "mysql_connection.h"
#include <cppconn/driver.h>
#include <cppconn/exception.h>
#include <cppconn/resultset.h>
#include <cppconn/statement.h>
#include <cppconn/prepared_statement.h>
#include <curl/curl.h>

using namespace std;


class cls_cmd {
public:
	//propiedades

	int id;
	string nombre; //nombre
	string direccion_ip;
	int puerto;
	int direccion_esclavo;
	string interface1;
	int velocidad;
	string paridad;
	int bitsparada;
	string equipo;
	string url_miner;
	string usuario;
	string password;
	bool habilitar;
	string protocolo;

	int registro;
	int clsvalor;
	bool resetear_s9;
	string datetime;
	bool hab_ejecutar;
	string lote;

	// metodos

	int ejecutar(void);
	int cmdresetear_s9(void);
	int cmdescribir_serial(void);
	int cmdescribir_tcp(void);

};



#endif
