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

// retorna 1 si reseteo el minador y -1 si hay error
int cls_cmd::cmdresetear_s9(void) {
	CURL* curl;
	FILE* fp;
	FILE* fp2;
	CURLcode result;
	char* urlminador;
	char* archivo;
	char* usuariopass;
	char caracter;
	string buffer1;
	int rc = -1;
	urlminador = (char*)calloc(256, sizeof(char));
	archivo = (char*)calloc(32, sizeof(char));
	usuariopass = (char*)calloc(32, sizeof(char));
	sprintf(urlminador, "http://%s/cgi-bin/reboot.cgi", direccion_ip.c_str());
	sprintf(usuariopass, "%s:%s", usuario.c_str(), password.c_str());
	sprintf(archivo, "logreset%s.txt", direccion_ip.c_str());
	string valoretiquetastr;
	fp = fopen(archivo, "wb");
	curl = curl_easy_init();
	if (curl) { // inicio del if 1
		curl_easy_setopt(curl, CURLOPT_URL, urlminador);
		curl_easy_setopt(curl, CURLOPT_WRITEDATA, fp);
		curl_easy_setopt(curl, CURLOPT_FAILONERROR, 1L);
		curl_easy_setopt(curl, CURLOPT_USERPWD, usuariopass);
		curl_easy_setopt(curl, CURLOPT_HTTPAUTH, CURLAUTH_ANY);
		result = curl_easy_perform(curl);

		if (result == CURLE_OK) { //inicio del if 2
			printf("%s - reset del ant miner s9 ok! | url:%s |user:%s \n", direccion_ip.c_str(), urlminador, usuariopass);
			rc = 1;
		}

		else
		{ // 3
			printf("%s - reset del ant miner - error %s \n", direccion_ip.c_str(), curl_easy_strerror(result));
			return -1;
		}; // fin 3
	};		// fin del if 1
	fclose(fp);

	curl_easy_cleanup(curl);

	return rc;
}

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
	if (protocolo.compare("HTML_S9") == 0) {
		res = cmdresetear_s9();  // devuelve un 1 su todo ok
	}
	else if (protocolo.compare("MBSERIAL") == 0) {
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


