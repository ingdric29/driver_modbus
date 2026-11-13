/*
programa dedarrollado por ing. Alejandro diomede - Automatroni - email: diomede3009@gmail.com
Define la estructura de datos llamada Equipos - tcp y serial
*/

#include "include/cls_cmd.h"
#include "include/conversiones.h"
#include <errno.h>
#include <vector>
#include <ctype.h>
#include <modbus.h>


using namespace std;


extern void esperar(double segundos);
extern uint32_t G_modbus_timeout;



// retorna 1 si tiene exito escribiendo un registro, sino retorna -1
int cls_cmd::cmdescribir_serial(void) {
	// declaracion de variables para uso con el protocolo modbus
	modbus_t* ctx_tcp = NULL;
	int rc = -1;
	int intentosmax = 3;
	int intento = 0;
	int resultado_escribir = false;
	ctx_tcp = modbus_new_rtu(interface1.c_str(), velocidad, paridad.c_str()[0], 8, bitsparada);
	if (ctx_tcp == NULL) { // if 1
		fprintf(stderr, "=cmdescribir:falla en iniciar libmodbus\n");
		resultado_escribir = false;
		return -1;
	} // fin if 1
	if (direccion_esclavo == 0) direccion_esclavo = 1;
	modbus_set_slave(ctx_tcp, direccion_esclavo);
	if (modbus_connect(ctx_tcp) == -1) { // if 3
		fprintf(stderr, "cmdescribir:conexion ha fallado: %s\n", modbus_strerror(errno));
		modbus_free(ctx_tcp);
		resultado_escribir = false;
		return -1;
	} // fin del if 3
	modbus_set_response_timeout(ctx_tcp, 0, G_modbus_timeout);
	while ((intento < intentosmax) && (rc == -1)) { //while 1
		  //cout << "la funcion es: " << funcion << endl;
		  //cout << "la interpretacion es: " << interpretacion << endl;
		intento++;
		if (intento > 1) { esperar(0.1); };
		rc = modbus_write_register(ctx_tcp, registro, clsvalor);
	} // fin del while 1

	modbus_close(ctx_tcp);
	modbus_free(ctx_tcp);
	return rc;
}

// retorna 1 si tiene exito escribiendo un registro, sino retorna -1
int cls_cmd::cmdescribir_tcp(void) {
	//cout << "escribir registro tcp" << endl;
	modbus_t* ctx_tcp = NULL;
	int rc = -1;
	int intentosmax = 3;
	int intento = 0;
	int resultado_escribir = false;
	ctx_tcp = modbus_new_tcp(direccion_ip.c_str(), puerto);
	if (ctx_tcp == NULL) { // if 1
		fprintf(stderr, "=cmdescribir_tcp:falla en iniciar libmodbus\n");
		resultado_escribir = false;
		return -1;
	} // fin if 1
	if (direccion_esclavo == 0) direccion_esclavo = 1;
	modbus_set_slave(ctx_tcp, direccion_esclavo);
	if (modbus_connect(ctx_tcp) == -1) { // if 3
		fprintf(stderr, "cmdescribir_tcp:conexion ha fallado: %s\n", modbus_strerror(errno));
		modbus_free(ctx_tcp);
		resultado_escribir = false;
		return -1;
	} // fin del if 3

	while ((intento < intentosmax) && (rc == -1)) { //while 1
		  //cout << "la funcion es: " << funcion << endl;
		  //cout << "la interpretacion es: " << interpretacion << endl;
		intento++;
		if (intento > 1) { esperar(0.1); };
		rc = modbus_write_register(ctx_tcp, registro, clsvalor);
		//cout << "cmdescribir_tcp | registro: " << registro << " valor: " << clsvalor << endl; 
	} // fin del while 1
	modbus_close(ctx_tcp);
	modbus_free(ctx_tcp);
	return rc;



}


int cls_cmd::ejecutar(void) {
	int res = -1;
	int resultadofinal = 0;
	//cout << "comando ejecutar" << endl;
	if (protocolo.compare("MBSERIAL") == 0) {
		res = cmdescribir_serial();
	}
	else if (protocolo.compare("MBTCP") == 0) {
		res = cmdescribir_tcp();
	}
	else {
		res = -1;
	}

	if (res > 0) {
		resultadofinal = 0;
	}
	else {
		resultadofinal = res;
	}

	return resultadofinal;

}


