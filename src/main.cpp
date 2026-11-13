/*
programa principal dedarrollado por ing. Alejandro diomede - sigge - email: diomede3009@gmail.com

g++ -o pmlog main.cpp loadconfig.cpp ini.c INIReader.cpp cls_equipos.cpp muestreador.cpp -I/usr/local/include/modbus -L/usr/local/lib -lmodbus
g++ -o devlogplus main.cpp loadconfig.cpp ini.c INIReader.cpp cls_equipos.cpp -I/usr/local/include/modbus -L/usr/local/lib -lmodbus


Ultima revision: 15/11/24
Hecha por: Javier Flores

	+Se añade la clase Logger para registrar eventos en la ejecucion del programa. 

*/

#include <iostream>
#include <string>
#include <time.h>
#include "include/cls_equipos.h"
#include "include/cls_cmd.h"
#include "include/conversiones.h"
#include "include/cls_defalarmas.h"
#include "include/cls_email.h"
#include "include/cls_defeventos.h"
#include <thread>
#include <iomanip>
#include <vector>
#include <stack>
#include <sstream>
#include "include/globales.h"
#include "include/estructuras.h"
#include <chrono>
#include "include/worker_group.h"
#ifdef _WIN32
#include <Windows.h>
#else
#include <unistd.h>
#endif
#include "include/rs232.h"

#include "include/cls_logger.h"
Logger logger_main;

using namespace std;

// variables Globales  

cls_equipos* equipo_mbtcp; // se instancias la clase
cls_equipos* equipo_mbserial;
cls_equipos* equipo_rs232;
cls_equipos* equipo_rs232_distintos;
cls_defalarmas* defalarmas;
cls_cmd* cmd; // se instancias la clase
cls_email* emaillist; //directorio
cls_defeventos* defeventos;
cls_sms* smslist; // dista de numeros de telefonos
thread* hilosseriales; //puntero de arrego de hilos seriales


// definicion de estructuras para almacenar parametros

stdatabase stdblocal;
stdatabase stdbexterna;

storigen GLOBAL_email_origen;

int GLOBAL_contador_equipos_mbtcp;
int GLOBAL_contador_equipos_mbserial;
int GLOBAL_contador_equipos_rs232;
int GLOBAL_contador_equipos_rs232_distintos;
int GLOBAL_contador_defalarmas;
int GLOBAL_contador_defeventos;
int GLOBAL_contador_cmd;
int GLOBAL_contador_directorio;
int GLOBAL_contador_numerossms;
std::atomic<bool> GLOBAL_EJECUTARHILOS{true};
std::recursive_mutex GLOBAL_command_mutex;
uint32_t G_modbus_timeout = 90000;
//////////////////// Correccion.

///////////////////
int GLOBAL_contador_equipos; // indica cuantos equipos se estan leyendo
int GLOBAL_contador_comandos; // indica cuantos comandos hay en la tabla de tbcmd
int GLOBAL_contadorext_comandos; // indica cuantos comandos hay en la tabla de tbcmd con orden de ejecutar en base de datos externa
long GLOBAL_errores;
long GLOBAL_errores_ultimo;
time_t GLOBAL_tiempo_actual;
time_t GLOBAL_ultimo_tiempo_tbejecutar_dblocal;
time_t GLOBAL_ultimo_tiempo_tbejecutar_dbexterna;
struct tm* GLOBAL_timeinfo;

string GLOBAL_db_direccion;
string GLOBAL_db_puerto;
string GLOBAL_db_usuario;
string GLOBAL_db_clave;
string GLOBAL_db_nombre;
double GLOBAL_periodo_muestreo_tbejecutar;   // se usa para ejecutar los comandos en la tabla tbcmd
double GLOBAL_periodo_muestreo_tbcambios; // se usa para detectar cambios en la configuracion

string GLOBAL_dbext_direccion;
string GLOBAL_dbext_puerto;
string GLOBAL_dbext_usuario;
string GLOBAL_dbext_clave;
string GLOBAL_dbext_nombre;
double GLOBAL_periodoext_muestreo_tbejecutar;

double GLOBAL_periodo_revision_alarmas;
double GLOBAL_ultima_revision_alarmas;

string GLOBAL_nombre_concentrador;
string GLOBAL_picos_tags;

double GLOBAL_periodo_minimo_escritura_tbvalores;
double GLOBAL_periodo_minimo_escritura_tbvalores_ex;
double GLOBAL_ultimotiempo_escritura_tbvalores;
double GLOBAL_periodo_revision_tablas_locales;
double GLOBAL_periodo_revision_tablas_externas;
double GLOBAL_ultima_revision_tablas_locales;
double GLOBAL_ultima_revision_tablas_externas;

double GLOBAL_maximo_dias_antiguedad_tbvalores;
double GLOBAL_maximo_dias_antiguedad_tbinstantaneos;
double GLOBAL_maximo_dias_antiguedad_tbvalores_ext;
double GLOBAL_maximo_dias_antiguedad_tbinstantaneos_ext;

double GLOBAL_sicronizacion_tiempo;
double GLOBAL_sicronizacion_periodo;
int GLOBAL_cantidadregistrossubir;

int GLOBAL_cont_cambio_tablacom;
time_t GLOBAL_tbcambio_tiempo_ultimochequeo;

string separadorigual(80, 61);
string separadorguion(80, 45);
double G_diferencia = 0.0;

string cls_equipos::clsarchivo = "";
time_t cls_equipos::clsarchivo_tiempo = time(NULL);

string GLOBAL_direccionip_dns;

double GLOBAL_periodo_reseteo_mysqlexterno;
double GLOBAL_ultimo_reseteo_mysqlexterno;

// GLOBAL_periodo_revision_tablas_externas 
// int ordenlecturaconfiguracionciclico(), en este metodo despues de cada
// periodo de revision externa, se vuelve hacer GLOBAL_ACCESO_EXTERNO = true
// y en cada fallo de acceso MYSQL se coloca GLOBAL_ACCESO_EXTERNO = false
std::atomic<bool> GLOBAL_ACCESO_EXTERNO{true};
int GLOBAL_habilitar_acceso_externo = 1; //0:no 1:si
int GLOBAL_habilitar_imprimir = 0;

double GLOBAL_periodo_minimo_escritura_tbdispositivos_sta = 5.0;
double GLOBAL_tiempo_ultimaescritura_tbdispositivos_sta = 0.0;

//1=primera corrida en main
//2=cargados todos los hilos de ejecucion
//3=ya con valores leidos
std::atomic<int> GLOBAL_estado_primera_ejecucion_programa{0};

double GLOBAL_periodo_revision_eventos = 4;
double GLOBAL_tiempo_ultima_revision_eventos = 0;

double GLOBAL_maxima_cuenta_errores = 50;
double GLOBAL_tiempo_restablecimiento_comunicacion = 60;

string GLOBAL_ultimomensajemostrado = "";
std::recursive_mutex GLOBAL_output_mutex;

string GLOBAL_clave1;
string GLOBAL_clave2;

/*
============================================================
MAIN
============================================================
*/

int main(int argc, char* argv[]) {
	int res = 0;
	long long valorclave1 = 0;
	long long valorclave2 = 0;

	if (argc > 1) GLOBAL_habilitar_imprimir = atoi(argv[1]);
	cout << separadorigual << endl;
	cout << "AUTOMONITOR DRIVER V3.6 ";
	cout << "AUTOMATRONI C.A. visitanos: www.automatroni.com" << endl;
	cout << "Rev. 1.1.2  26/11/24" << endl;
	cout << separadorigual << endl;

	if (loadconfigdb()) {
		cout << separadorguion << endl;
		cout << "archivo de configuracion de base de datos cargado!" << endl;
		logger_main.registrarError("INFO: Devlog initialized. Configuration file loaded.");
	}
	else {
		cout << "falla en el archivo de configuracion configdb.cfg!" << endl;
		logger_main.registrarError("ERROR: Configuration file configdb.cfg failed to load!");
	};

	stdblocal.direccion = GLOBAL_db_direccion;
	stdblocal.puerto = cast_string_to_int(GLOBAL_db_puerto);
	stdblocal.nombre = GLOBAL_db_nombre;
	stdblocal.usuario = GLOBAL_db_usuario;
	stdblocal.clave = GLOBAL_db_clave;

	stdbexterna.direccion = GLOBAL_dbext_direccion;
	stdbexterna.puerto = cast_string_to_int(GLOBAL_dbext_puerto);
	stdbexterna.nombre = GLOBAL_dbext_nombre;
	stdbexterna.usuario = GLOBAL_dbext_usuario;
	stdbexterna.clave = GLOBAL_dbext_clave;

	string arrcampos[] = { "activas","reconocida" };
	string arrvalores[] = { "1","1" };
	//int res_act=0;

	GLOBAL_ultima_revision_tablas_locales = time(NULL);
	GLOBAL_ultimo_reseteo_mysqlexterno = 0;
	GLOBAL_contador_defeventos = -1;

	valorclave1 = std::stoull(GLOBAL_clave1,NULL,16);
	valorclave2 = std::stoull(GLOBAL_clave2,NULL,16);
	if (!(check_key(valorclave1) || check_key(valorclave2))) {
		cout << "Ambas claves invalidas" << endl;
		cout << "favor suministrar los siguientes codigos" << endl;
		cout << "a su vendedor para conseguir sus claves:" << endl;
		logger_main.registrarError("ERROR: INVALID KEYS DETECTED! Please contact your provider to obtain the correct keys. License not activated. Devlog stopped.");
		listainterfaces();
		insertar_eventos_tbhistoalarmas("licencia no activada!","");
		esperar(60);
		return 1;
	};	
	insertar_eventos_tbhistoalarmas("registrador iniciado!","");
	logger_main.registrarError("INFO: Start Devlog.");
	muestrear();
	return 0;
};


void esperar(double segundos) {
	double milisegundos;
	milisegundos = 1000 * segundos;

	std::this_thread::sleep_for(std::chrono::milliseconds((int) milisegundos));
	
	
}


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
// 1 consiguio etiqueta y -1 no consiguio etiqueta

/*
==================================================
MUESTREO
==================================================
*/

int muestrear(void) {
	int i = 0;
	int resultado = -1;
	string comando;
	int res_ejecutar_externa = -1;

	double tbcambio_diferencia_tiempo = 0;
	time_t tbcambio_tiempo_ultimochequeo = 0;
	bool hilosenejecucion = false;

	GLOBAL_estado_primera_ejecucion_programa = 1;
	cout << "empezar a ejecutar hilos" << endl;
	while (1) {
		if (ordenlecturaconfiguracion() < 0) { liberarmemoria(); return -1; }
		GLOBAL_EJECUTARHILOS = true;
		actualizar_valor_condicion(&stdblocal, "tbdispositivos_sta", "estado", "0", "`id`>'0'");
        WorkerGroup workers(GLOBAL_EJECUTARHILOS);
        workers.start(ordenlecturaconfiguracionciclico);
        workers.start(ordenejecucionciclico);
        workers.start(ordenlecturaequiposciclico, equipo_mbtcp, GLOBAL_contador_equipos_mbtcp);
        workers.start(ordenlecturaequiposciclico, equipo_mbserial, GLOBAL_contador_equipos_mbserial);
        workers.start(ordenlecturaequiposciclico, equipo_rs232, GLOBAL_contador_equipos_rs232);
        workers.start(ordenrevisarposiblesalarmasciclico);
        workers.start(orden_actualizarvalores_clsdefeventos);
        workers.start(ordenescrituraequiposciclico);
        GLOBAL_estado_primera_ejecucion_programa = 2;
        for (int serial = 0; serial < GLOBAL_contador_equipos_rs232_distintos; ++serial)
            workers.start(lectura_puerto_serial, &equipo_rs232_distintos[serial]);
        workers.join();

		cout << separadorigual << endl;
		
		esperar(5);
		liberarmemoria();

	}
}


int ordenejecucuionlocal() {
	std::lock_guard<std::recursive_mutex> commands(GLOBAL_command_mutex);
	int res_cmd_local = 0;
	int res_leer_tbejecutar_local = -1;
	time_t res_segundos;
	int res_ejecutar = -1;
	string res_lote = "";
	//orden de ejecucion local
	res_leer_tbejecutar_local = leer_tbejecutar(res_cmd_local, res_segundos, res_lote, GLOBAL_db_direccion, getVal(GLOBAL_db_puerto), GLOBAL_db_nombre, GLOBAL_db_usuario, GLOBAL_db_clave);
	//cout << "valor de res_lote local:" << res_lote << endl;
	//cout << "orden local de ejecucion: " << res_cmd_local << " : " << cast_time_to_datetime (time(NULL)) << endl;
	imprimir_mensaje("cmd local result = " + cast_int_to_string(res_cmd_local), amarillo);
	if (res_cmd_local == 1) {
		GLOBAL_contador_comandos = leer_tbcmd(GLOBAL_db_direccion, getVal(GLOBAL_db_puerto), GLOBAL_db_nombre, GLOBAL_db_usuario, GLOBAL_db_clave);
		//cout << "Contador de cmd = " << GLOBAL_contador_comandos << endl;
		if (GLOBAL_contador_comandos > 0) {

			for (int j = 0; j < GLOBAL_contador_comandos; j++) {
				if (cmd[j].hab_ejecutar == true) {
					if (cmd[j].lote.compare(res_lote) == 0) {
						//cout << "ejecucion local lote:" << cmd[j].lote << " | res_lote:" << res_lote << endl;
						//cout << "registro:" << cmd[j].registro << " | valor:" << cmd[j].clsvalor << endl; 
						res_ejecutar = cmd[j].ejecutar();
						
						if (res_ejecutar > 0) {
							escribir_tbcmd_fechahora(cmd[j].id, GLOBAL_db_direccion, getVal(GLOBAL_db_puerto), GLOBAL_db_nombre, GLOBAL_db_usuario, GLOBAL_db_clave);
						}
					}
				}
			}; // fin del for
		};
	};

	return res_ejecutar;
}



int ordenejecucuionexterna() {
	std::lock_guard<std::recursive_mutex> commands(GLOBAL_command_mutex);
	int res_leer_tbejecutar_externa = -1;
	int res_cmd_externa = 0;
	time_t res_segundos;
	int res_ejecutar_externa = -1;
	string res_lote = "";
	// orden de jecucion externa desde una base de datos en la nube	  
	res_leer_tbejecutar_externa = leer_tbejecutar(res_cmd_externa, res_segundos, res_lote, GLOBAL_dbext_direccion, getVal(GLOBAL_dbext_puerto), GLOBAL_dbext_nombre, GLOBAL_dbext_usuario, GLOBAL_dbext_clave);
	//cout << "valor de res_lote externo:" << res_lote << endl;
	//cout << "orden externa de ejecucion: " << res_cmd_externa << " : " << cast_time_to_datetime (time(NULL)) << endl;
	imprimir_mensaje("cmd externo result = " + cast_int_to_string(res_cmd_externa), amarillo);
	if (res_cmd_externa == 1) {
		GLOBAL_contadorext_comandos = leer_tbcmd(GLOBAL_dbext_direccion, getVal(GLOBAL_dbext_puerto), GLOBAL_dbext_nombre, GLOBAL_dbext_usuario, GLOBAL_dbext_clave);
		if (GLOBAL_contadorext_comandos > 0) {
			
			for (int j = 0; j < GLOBAL_contadorext_comandos; j++) {
				if (cmd[j].hab_ejecutar == true) {
					if (cmd[j].lote.compare(res_lote) == 0) {
						//cout << "ejecucion externo lote:" << cmd[j].lote << " | res_lote:" << res_lote << endl;
						//cout << "registro:" << cmd[j].registro << " | valor:" << cmd[j].clsvalor << endl; 
						res_ejecutar_externa = cmd[j].ejecutar();
						if (res_ejecutar_externa > 0) {
							escribir_tbcmd_fechahora(cmd[j].id, GLOBAL_dbext_direccion, getVal(GLOBAL_dbext_puerto), GLOBAL_dbext_nombre, GLOBAL_dbext_usuario, GLOBAL_dbext_clave);
						}
					}
				}
			};	// fin del ford
		};
	};
	//cout << "fin orden ejecucion externa" << endl;
	return res_ejecutar_externa;
}


int ordenejecucionciclico() {
	double diferencia1 = 0.0;
	double diferencia2 = 0.0;
	while (GLOBAL_EJECUTARHILOS) {
		diferencia1 = difftime(time(NULL), GLOBAL_ultimo_tiempo_tbejecutar_dblocal);
		if (diferencia1 > GLOBAL_periodo_muestreo_tbejecutar) { // if 1
			GLOBAL_ultimo_tiempo_tbejecutar_dblocal = time(NULL);
			ordenejecucuionlocal();
		}
		diferencia2 = difftime(time(NULL), GLOBAL_ultimo_tiempo_tbejecutar_dbexterna);
		if (diferencia2 > GLOBAL_periodoext_muestreo_tbejecutar) { // if 1
			GLOBAL_ultimo_tiempo_tbejecutar_dbexterna = time(NULL);
			ordenejecucuionexterna();
			//cout << "periodoext: " << GLOBAL_periodoext_muestreo_tbejecutar << endl;
			//cout << "diferencia2: " << diferencia2 << endl;
		}
		int tiempominimo = 0;
		if (GLOBAL_periodo_muestreo_tbejecutar <= GLOBAL_periodoext_muestreo_tbejecutar) {
			tiempominimo = GLOBAL_periodo_muestreo_tbejecutar;
		}
		else {
			tiempominimo = GLOBAL_periodoext_muestreo_tbejecutar;
		}
		std::this_thread::sleep_for(std::chrono::seconds(tiempominimo));
	}
	cout << " fin hilo lectura ordenes de ejecucion " << endl;
	return 1;
}


// lee valores de los equipos indicados y los inserta en las bases de datos locales y externa
int ordenlecturaequipos(cls_equipos* equipo, int arge_cantidad) {
	/* ORDENES DE LECTURAS DE LOS DISPOSITIVOS SEGUN TABLECOM*/
	double diferencia = 0.0;
	int resultado=-1;
	try {
		for (int i = 0; i < arge_cantidad; i++) {
            std::lock_guard<std::recursive_mutex> state(*equipo[i].state_mutex); // for 1

			const auto now = PeriodicDeadline::Clock::now();
			resultado = -1;
			if (equipo[i].read_deadline.due(now)) {
				//cout << "*";
				resultado = equipo[i].leer();
                equipo[i].read_deadline.advance(equipo[i].periodo);

			} // end if 1 */

		} // end for
		GLOBAL_estado_primera_ejecucion_programa = 3;
		return resultado;
	}
	catch (...) {
		imprimir_mensaje("falla ordenlecturaequipos");

		std::string mensajeError = std::string("DEBUG: ordenlecturaequipos fail!") +
                               " | File: " + __FILE__ +
                               " | Function: " + __FUNCTION__ +
                               " | Line: " + std::to_string(__LINE__);

    	logger_main.registrarError(mensajeError);
		return -1;
	}
}
//Correccion: Se habilita y deshabilita la escritura en la base de datos en base a un error de comunicacion.
//Fecha de Correccion: 30/10/24
int ordenlecturaequiposciclico(cls_equipos* equipo, int arge_cantidad) {
	int res_ordenlecturaequipos;
	bool iniciado = false;
	if (arge_cantidad) {cout << " iniciado hilo con protocolo: " << equipo[0].protocolo << endl;};
	long contador = 0;
	try
	{
		while (GLOBAL_EJECUTARHILOS) {
			//contador++; //Que hace esta variable??¿?¿?¿?¿
			if (arge_cantidad == 0) 
			{
				cout << "no hay equipos" << endl;
				break;
			};
			res_ordenlecturaequipos = ordenlecturaequipos(equipo, arge_cantidad);
			//printf("EJECUTAR_HILOS = %d CONTADOR = %d \n",GLOBAL_EJECUTARHILOS,contador);
			iniciado = true;

			auto wake = PeriodicDeadline::Clock::now() + std::chrono::milliseconds(200);
            for (int tag = 0; tag < arge_cantidad; ++tag) {
                std::lock_guard<std::recursive_mutex> state(*equipo[tag].state_mutex);
                wake = std::min(wake, equipo[tag].read_deadline.next());
            }
            std::this_thread::sleep_until(wake);
		}
		if (iniciado){ 
		cout << " fin hilo lectura variables protocolo " << equipo[0].protocolo << endl;};
		return 1;	
	}
	catch (...){

		cout << "falla en ordenlecturaequiposciclico" << endl;
		
		std::string mensajeError = std::string("DEBUG: ordenlecturaequiposciclico fail!")  +
                               " | File: " + __FILE__ +
                               " | Function: " + __FUNCTION__ +
                               " | Line: " + std::to_string(__LINE__);

    	logger_main.registrarError(mensajeError);
		return 2;
	}
		
}


//ordenrevisarposiblesalarmasciclico
int ordenrevisarposiblesalarmasciclico() {
	int res_ordenrevisarposiblesalarmasciclico;
	while (GLOBAL_EJECUTARHILOS) {
		double diferencia_tiempoalarmas = 0.0;
		diferencia_tiempoalarmas = difftime(time(NULL), GLOBAL_ultima_revision_alarmas);
		if (diferencia_tiempoalarmas >= GLOBAL_periodo_revision_alarmas) {
			res_ordenrevisarposiblesalarmasciclico = ordenrevisarposiblesalarmas();
		}
		int tiempoentero = (int)(GLOBAL_periodo_revision_alarmas);
		std::this_thread::sleep_for(std::chrono::seconds(tiempoentero));
	}
	cout << " fin hilo ordenrevisarposiblesalarmasciclico " << endl;
	return 1;
}

int ordenrevisarposiblesalarmas() {

	imprimir_mensaje("inicio revision tabla de alarmas: " + cast_int_to_string(GLOBAL_contador_defalarmas), negro);
	for (int j = 0; j < GLOBAL_contador_defalarmas; j++) {
		defalarmas[j].evaluarcompleto();
	}
	imprimir_mensaje("terminada revision tabla de alarmas", negro);
	GLOBAL_ultima_revision_alarmas = time(NULL);
	return 1;

}


int ordenlecturaconfiguracion() {
    // Coordinator only: no workers exist while definitions are replaced.
    string campo2[5];
    lee_tbcambios();
    {
		if (actualizar_leer_equipo(MBTCP) < 0) return -1;
		if (actualizar_leer_equipo(MBSERIAL) < 0) return -1;
		if (actualizar_leer_equipo(RS232) < 0) return -1;
		if (leer_tbcmd(GLOBAL_db_direccion, getVal(GLOBAL_db_puerto), GLOBAL_db_nombre, GLOBAL_db_usuario, GLOBAL_db_clave) < 0) return -1;
		if (actualizar_defalarmas(&stdblocal) < 0) return -1;
		if (actualizar_directorio(&stdblocal) < 0) return -1;
		if (actualizar_smslist(&stdblocal) < 0) return -1;
		if (actualizar_defeventos(&stdblocal) < 0) return -1;
		actualizar_posiciones_clsdefeventos();

		//campo2[0] = "id";
		//campo2[1] = "activar";
		//campo2[2] = "desactivar";
		//campo2[3] = "descripcion";
		//replicaciontablaselectiva(&stdblocal, "tbdefalarmas", &stdblocal, "tbalarmas", &campo2[0], 4, 256); 

		campo2[0] = "id";
		campo2[1] = "nombre";
		campo2[2] = "";
		campo2[3] = "";
		replicaciontablaselectiva(&stdblocal, "tbdispositivos", &stdblocal, "tbdispositivos_sta", &campo2[0], 2, 256);

		actualizar_valor_condicion(&stdblocal, "tbdispositivos_sta", "concentrador", GLOBAL_nombre_concentrador, "id >= 0");

		imprimir_mensaje("se leyeron las configuraciones", verde);
	}
	return 1;
}


int ordenlecturaconfiguracionciclico() {

	double tbcambio_diferencia_tiempo = 0.0;
	double diferencia_tiempo3 = 0.0;
	double diferencia_tiempo4 = 0.0;
	double diferencia_tiempo5 = 0.0;
	double diferencia_tiempo6 = 0.0;
	double diferencia1 = 0.0;
	double diferencia2 = 0.0;

	while (GLOBAL_EJECUTARHILOS) {
		// orden de lectura de configuracion
		tbcambio_diferencia_tiempo = difftime(time(NULL), GLOBAL_tbcambio_tiempo_ultimochequeo);
		//cout << tbcambio_diferencia_tiempo << " - " << GLOBAL_periodo_muestreo_tbcambios << endl;
		if (tbcambio_diferencia_tiempo >= GLOBAL_periodo_muestreo_tbcambios) {
			GLOBAL_tbcambio_tiempo_ultimochequeo = time(NULL);
			//cout << "se lee la configuracion" << endl;
			imprimir_mensaje("revision tabla tbcambios", negro);
			if (lee_tbcambios() == 1) { GLOBAL_EJECUTARHILOS = false; break; }
		}


		// borrar registros antiguos locales
		diferencia_tiempo3 = difftime(time(NULL), GLOBAL_ultima_revision_tablas_locales);
		if (diferencia_tiempo3 > GLOBAL_periodo_revision_tablas_locales) {
			GLOBAL_ultima_revision_tablas_locales = time(NULL);
			imprimir_mensaje("orden borrar registros locales", azul);
			ordenborrarregistroslocales();
		}
		// borrar registros antiguos externos		
		diferencia_tiempo4 = difftime(time(NULL), GLOBAL_ultima_revision_tablas_externas);
		if (diferencia_tiempo4 > GLOBAL_periodo_revision_tablas_externas) {
			GLOBAL_ultima_revision_tablas_externas = time(NULL);
			imprimir_mensaje("orden borrar registros externos", azul);
			ordenborrarregistrosexternos();

		}

		//sincronizar tablas
		diferencia_tiempo5 = difftime(time(NULL), GLOBAL_sicronizacion_tiempo);
		if (diferencia_tiempo5 > GLOBAL_sicronizacion_periodo) {
			GLOBAL_sicronizacion_tiempo = time(NULL);
			imprimir_mensaje("sincronizar tbvalores", amarillo);
			sincronizar_tablas(&stdblocal, &stdbexterna, "tbvalores", GLOBAL_cantidadregistrossubir);
		}

		//reseteo falla mysql externo
		diferencia_tiempo6 = difftime(time(NULL), GLOBAL_ultimo_reseteo_mysqlexterno);
		if (diferencia_tiempo6 > GLOBAL_periodo_reseteo_mysqlexterno) {
			GLOBAL_ultimo_reseteo_mysqlexterno = time(NULL);
			imprimir_mensaje("se ha activado, la comunicacion con mysql externo", amarillo);
			GLOBAL_ACCESO_EXTERNO = true;

		}
		int tiempoentero = (int)(GLOBAL_periodo_muestreo_tbcambios);
		esperar(tiempoentero);
	}
	imprimir_mensaje("final hilo leer tabla cambios ");
	imprimir_mensaje("fin hilo lectura ordenes de ejecucion ");
	imprimir_mensaje("final hilo revision tablas locales y externas ");
	imprimir_mensaje("final hilo sicronizar tablas");

	return 1;

}




int ordenborrarregistroslocalesciclico() {

	double diferencia_tiempo = 0.0;

	while (GLOBAL_EJECUTARHILOS) {
		diferencia_tiempo = difftime(time(NULL), GLOBAL_ultima_revision_tablas_locales);
		if (diferencia_tiempo > GLOBAL_periodo_revision_tablas_locales) {
			GLOBAL_ultima_revision_tablas_locales = time(NULL);
			ordenborrarregistroslocales();
		}
		int tiempoentero = (int)(GLOBAL_periodo_revision_tablas_locales);
		esperar(tiempoentero);
	}
	imprimir_mensaje("final hilo revision tablas locales ");
	return 1;

}


int ordenborrarregistrosexternosciclico() {

	double diferencia_tiempo = 0.0;

	while (GLOBAL_EJECUTARHILOS) {
		diferencia_tiempo = difftime(time(NULL), GLOBAL_ultima_revision_tablas_externas);
		if (diferencia_tiempo > GLOBAL_periodo_revision_tablas_externas) {
			GLOBAL_ultima_revision_tablas_externas = time(NULL);
			ordenborrarregistrosexternos();
		}
		int tiempoentero = (int)(GLOBAL_periodo_revision_tablas_externas);
		esperar(tiempoentero);
	}
	imprimir_mensaje("final hilo revision tablas externas ");
	return 1;

}

int ordenborrarregistroslocales() {

	//borrar_registros(GLOBAL_maximo_dias_antiguedad_tbinstantaneos, GLOBAL_db_direccion, getVal(GLOBAL_db_puerto), GLOBAL_db_nombre, GLOBAL_db_usuario, GLOBAL_db_clave, "tbinstantaneos");
	borrar_registros(GLOBAL_maximo_dias_antiguedad_tbvalores, GLOBAL_db_direccion, getVal(GLOBAL_db_puerto), GLOBAL_db_nombre, GLOBAL_db_usuario, GLOBAL_db_clave, "tbvalores");
	return 1;
}

int ordenborrarregistrosexternos() {

	//borrar_registros(GLOBAL_maximo_dias_antiguedad_tbinstantaneos_ext, GLOBAL_dbext_direccion, getVal(GLOBAL_dbext_puerto), GLOBAL_dbext_nombre, GLOBAL_dbext_usuario, GLOBAL_dbext_clave, "tbinstantaneos");
	borrar_registros(GLOBAL_maximo_dias_antiguedad_tbvalores_ext, GLOBAL_dbext_direccion, getVal(GLOBAL_dbext_puerto), GLOBAL_dbext_nombre, GLOBAL_dbext_usuario, GLOBAL_dbext_clave, "tbvalores");

	return 1;
}


int orden_sincronizar_tablas_ciclico() {

	double diferencia_tiempo = 0.0;

	while (GLOBAL_EJECUTARHILOS) {
		diferencia_tiempo = difftime(time(NULL), GLOBAL_sicronizacion_tiempo);
		if (diferencia_tiempo > GLOBAL_sicronizacion_periodo) {
			GLOBAL_sicronizacion_tiempo = time(NULL);
			imprimir_mensaje("sincronizar tbvalores. bdlocal=>bdexterna", azul);
			sincronizar_tablas(&stdblocal, &stdbexterna, "tbvalores", GLOBAL_cantidadregistrossubir);
		}
		int tiempoentero = (int)(GLOBAL_sicronizacion_periodo);
		esperar(tiempoentero);
	}
	cout << "final hilo sicronizar tablas" << endl;
	return 1;

}


void imprimir_mensaje(string str, GLOBAL_colores color) {
	std::lock_guard<std::recursive_mutex> output(GLOBAL_output_mutex);
	//imprimir_mensaje("\033[1;32mse leyeron las configuraciones\033[0m" );
	if (GLOBAL_habilitar_imprimir && (str.compare(GLOBAL_ultimomensajemostrado) != 0)) {
		if (color == nada) {
			cout << "[" << left << cast_time_to_datetime(time(NULL)) << "] - " << str << endl;
		}
		else {
			cout << "\033[1;" << color << "m" << "[" << left << cast_time_to_datetime(time(NULL)) << "] - " << str << "\033[0m" << endl;
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
		}
		else {
			cout << "\033[1;" << color << "m" << "[" << left << cast_time_to_datetime(time(NULL)) << "] - " << str << "\033[0m" << endl;
		}
	}
	GLOBAL_ultimomensajemostrado = str;
}




// dado el id, encuentra la posicion en memoria ram. cmd[j]
// es decir encuentra a j, en la expresion cmd[j] = id
int cmd_buscarid(int id) {
	int indice = -1;
	stringstream ss1;
	ss1 << "contador comandos:" << GLOBAL_contador_cmd;
	imprimir_mensaje(ss1.str());
	for (int j = 0; j < GLOBAL_contador_cmd; j++) {
		ss1.str("");
		ss1 << "valor id buscado=" << id << " | leido id=" << cmd[j].id << " | " << cmd[j].nombre << "indice ram=" << j;
		imprimir_mensaje(ss1.str());
		if (&cmd[j]) {
			if (id == cmd[j].id) {
				indice = j;
				break;
			}
		}
	}
	return indice;
}


// dado el id, encuentra la posicion en memoria ram. cmd[j]
// es decir encuentra a j, en la expresion cmd[j] = id
int email_buscarid(int id) {
	int indice = -1;
	for (int j = 0; j < GLOBAL_contador_directorio; j++) {
		if (&emaillist[j]) {
			if (id == emaillist[j].id) {
				indice = j;
				break;
			}
		}
	} //for
	return indice;
}


// dado el id, encuentra la posicion en memoria ram. cmd[j]
// es decir encuentra a j, en la expresion cmd[j] = id
int sms_buscarid(int id) {
	int indice = -1;
	for (int j = 0; j < GLOBAL_contador_numerossms; j++) {
		if (&smslist[j]) {
			if (id == smslist[j].clsid) {
				indice = j;
				break;
			}
		}
	} //for
	return indice;
}



int actualizar_posiciones_clsdefeventos() {

	if (GLOBAL_contador_defeventos == -1) {
		actualizar_defeventos(&stdblocal);
	}
	if (GLOBAL_contador_defeventos == -1) return 0;
	int ind = 0;
	while (ind < GLOBAL_contador_defeventos) {
		defeventos[ind].cargar_posicion();
		ind++;
	}

	return 0;
}

int actualizarvalores_clsdefeventos() {
	int ind = 0;
	if (GLOBAL_estado_primera_ejecucion_programa < 3) {
		return -1;
	}
	if (GLOBAL_contador_defeventos < 0) return -1;
	while (ind < GLOBAL_contador_defeventos) {
		defeventos[ind].leer_valorenram();
		ind++;
	}
	return 0;
}


int orden_actualizarvalores_clsdefeventos() {

	double diferencia_tiempo = 0.0;

	while (GLOBAL_EJECUTARHILOS) {
		diferencia_tiempo = difftime(time(NULL), GLOBAL_tiempo_ultima_revision_eventos);
		if (diferencia_tiempo > GLOBAL_periodo_revision_eventos) {
			GLOBAL_tiempo_ultima_revision_eventos = time(NULL);
			imprimir_mensaje("iniado revision tabla de eventos en ram", azul);
			actualizarvalores_clsdefeventos();
			imprimir_mensaje("terminada revision tabla de eventos en ram", azul);

		}
		int tiempoentero = (int)(GLOBAL_periodo_revision_eventos);
		esperar(tiempoentero);
	}
	cout << "final hilo revision de eventos" << endl;
	return 1;

}

void lectura_puertos_seriales() {
	int i=0;
	if (GLOBAL_contador_equipos_rs232_distintos > 0) {
		for (i = 0; i < GLOBAL_contador_equipos_rs232_distintos; i++) {
			hilosseriales[i] = std::thread(lectura_puerto_serial, &equipo_rs232_distintos[i]);
		}
	}
	else {
		return;
	}
}

void lectura_puerto_serial(cls_equipos* equipo_serial) {
	char modo[4];
	int numpuertoserial = -1;
	int velocidadserial = 9600;
	int controldeflujo = 0;
	int n;
	int local_numbits = 0;
	int tamanobuffer;
	
	try {
		local_numbits = equipo_serial->numerodebits;
		string nombrepuerto = equipo_serial->interface1;
		//nombrepuerto = cast_minus_mayus(nombrepuerto);
		numpuertoserial = RS232_GetPortnr(nombrepuerto.c_str());
		tamanobuffer = equipo_serial->tamanobuffer;

		modo[0] = char(local_numbits + 48);
		modo[1] = (char)toupper(equipo_serial->paridad[0]);
		modo[2] = char(equipo_serial->bitsparada + 48);
		modo[3] = 0;

		velocidadserial = equipo_serial->velocidad;
		controldeflujo = equipo_serial->controldeflujo;
		while (GLOBAL_EJECUTARHILOS) {  //while1

			if (RS232_OpenComport(numpuertoserial, velocidadserial, modo, controldeflujo)) // retorna 1 en caso de error
			{
				//falla en abrir puerto;
				imprimir_mensaje("falla en abrir puerto: (" + nombrepuerto + ")", rojo);
				logger_main.registrarError("ERROR: Failed to open port: (" + nombrepuerto + ")");

			}
			else {
				while (GLOBAL_EJECUTARHILOS) {
					{
                        std::lock_guard<std::mutex> serial(*equipo_serial->buffer_mutex);
                        n = RS232_PollComport(numpuertoserial, equipo_serial->buf, tamanobuffer);
                        *equipo_serial->p_n_buf = std::max(0, std::min(n, tamanobuffer));
                    }
					esperar(0.1); // espera 100 milisegundos
					//sleep(100) o usleep(100000)
				}

				RS232_CloseComport(numpuertoserial);
			}
			cout << "espere 10 seg para reabrir puerto serial (" << nombrepuerto << ") " << endl;
			std::this_thread::sleep_for(std::chrono::seconds(10));
		} // primer while 1

		return;
	}
	catch (...) {
		cout << "falla en modulo de rs232" << endl;

		std::string mensajeError = std::string("DEBUG: RS232 module fail!") +
                               " | File: " + __FILE__ +
                               " | Function: " + __FUNCTION__ +
                               " | Line: " + std::to_string(__LINE__);

    	logger_main.registrarError(mensajeError);
		return;

	}
	
}



// inserta en las bases de datos locales y externa
int ordenescrituraequipos(cls_equipos* equipo, int arge_cantidad) {
	/* ORDENES DE LECTURAS DE LOS DISPOSITIVOS SEGUN TABLECOM*/
	double diferencia = 0.0;
	double diferencia_instanteneos = 0.0;
	double diferencia_tbdispositivo_sta = 0.0;
	double diferencia_tbvalores = 0.0;
	double diferencia_tbvalores_ex = 0.0;
	int resultado = -1;
	int res_insertardb = -1;
	int res_insertardbext = -1;
	int res_evaluar = -1;
	bool res_leervalores = false;
	int solocambios;
	bool fueescrito;
	string campost[6];
	string valorst[6] = { "0","0" ,"0" ,"0" ,"0", "0" };
	string campoin[10];
	string valorin[10] = { "0","0" ,"0" ,"0" ,"0","0","0" ,"0" ,"0" ,"0" };

	campost[0] = "nombre";
	campost[1] = "errores";
	campost[2] = "errores_acumulados";
	campost[3] = "estado";
	campost[4] = "fechatiempo";
	campost[5] = "concentrador";
	valorst[5] = GLOBAL_nombre_concentrador;

	campoin[0] = "timedate";
	campoin[1] = "fechatiempo";
	campoin[2] = "tag";
	campoin[3] = "valorstr";
	campoin[4] = "valorfloat";
	campoin[5] = "dispositivo";
	campoin[6] = "rastro";
	campoin[7] = "concentrador";
	campoin[8] = "nombredispositivo";

	try {
		for (int i = 0; i < arge_cantidad; i++) {
            std::lock_guard<std::recursive_mutex> state(*equipo[i].state_mutex); // for 1

			const auto now = PeriodicDeadline::Clock::now();
			resultado = -1;
			//cout << "diferencia - periodo " << diferencia << "\t" << equipo[i].periodo << endl;
			if (equipo[i].instant_deadline.due(now)) {
				resultado = equipo[i].resultadoleer_rc;
				diferencia_tbdispositivo_sta = difftime(time(NULL), GLOBAL_tiempo_ultimaescritura_tbdispositivos_sta);
				if (equipo[i].status_deadline.due(now)) {
                    equipo[i].status_deadline.advance(GLOBAL_periodo_minimo_escritura_tbdispositivos_sta);
					valorst[0] = equipo[i].nombre;
					valorst[1] = cast_int_to_string(*equipo[i].pcontador_errores);
					valorst[2] = cast_int_to_string(*equipo[i].pcontador_errores_acumulados);
					valorst[3] = cast_bool_to_string(*equipo[i].pestadocomunicacion);
					valorst[4] = cast_time_to_datetime(time(NULL));
					// escribir conteo de errores
					GLOBAL_tiempo_ultimaescritura_tbdispositivos_sta = time(NULL);
					insertar_inteligente(&stdblocal, "tbdispositivos_sta", 0, &campost[0], &valorst[0], 6);
					//cout << "ordenescrituratbinstantaneo | periodo equipo:" << equipo[i].nombre << "resultado: " << resultado;
				}
				if (equipo[i].resultado_leer) { // if 2
					res_leervalores = true;
					valorin[0] = cast_time_to_datetime(time(NULL)); 	//"timedate" ;
					valorin[1] = valorin[0];							//"fechatiempo";
					valorin[2] = equipo[i].tag;  						//"tag";
					valorin[3] = equipo[i].clsvalorstr;  				//"valorstr";
					valorin[4] = cast_double_to_string(equipo[i].clsvalorfloat); 	//"valorfloat";
					valorin[5] = cast_int_to_string(equipo[i].id); 		//"dispositivo";
					valorin[6] = equipo[i].rastro;						//"rastro";
					valorin[7] = GLOBAL_nombre_concentrador; 			//"concentrador";
					valorin[8] = equipo[i].nombre; 						//"nombredispositivo";
					imprimir_mensaje("insertar bd local tabla tbinstantaneos: " + equipo[i].tag, negro);
					res_insertardb = insertar_inteligente3(&stdblocal, "tbinstantaneos", 2, 7, 8, &campoin[0], &valorin[0], 9);
					equipo[i].ultima_escritura_tbinstantaneo = time(NULL);
                    equipo[i].instant_deadline.advance(equipo[i].periodo);

				} // end del if 2 
			} // end if 1 */
			diferencia_tbvalores = difftime(time(NULL), equipo[i].ultimo_insertar);
			if ((equipo[i].resultado_leer) && (equipo[i].registrable)) {
				if (equipo[i].history_deadline.due(now)) { // if 3
					imprimir_mensaje("insertar bd local tabla tbvalores: " + equipo[i].tag, negro);
					if (habilitarescribir(equipo[i].fueescritodb, equipo[i].solocambios)) {
						bool stored = true;
                        for (const auto& sample : equipo[i].sample_window.samples(equipo[i].preservar_picos)) {
                            res_insertardb = insertardb(sample.acquired, equipo[i].tag, sample.text, sample.value, equipo[i].id, equipo[i].rastro, GLOBAL_nombre_concentrador, equipo[i].nombre, GLOBAL_db_direccion, getVal(GLOBAL_db_puerto), GLOBAL_db_nombre, GLOBAL_db_usuario, GLOBAL_db_clave, "tbvalores");
                            if (res_insertardb <= 0) { stored = false; break; }
                        }
                        if (!stored) continue;
                        equipo[i].sample_window.clear();
						equipo[i].ultimo_insertar = time(NULL);
                        equipo[i].history_deadline.advance(std::max(GLOBAL_periodo_minimo_escritura_tbvalores, equipo[i].periodo));
						equipo[i].fueescritodb = true;
					}
					if (res_insertardb == 0) {

					}
				} // if 3
			}
		} // end for
		GLOBAL_estado_primera_ejecucion_programa = 3;
		return res_insertardb;
	}
	catch (...) {
		imprimir_mensaje("falla ordenescrituraequipos");

		std::string mensajeError = std::string("DEBUG: ordenescrituraequipos fail!") +
                               " | File: " + __FILE__ +
                               " | Function: " + __FUNCTION__ +
                               " | Line: " + std::to_string(__LINE__);

    	logger_main.registrarError(mensajeError);
		return -1;
	}
}

// Correccion: Habilitacion y deshabilitacion de la escritura en la base de datos cuando se pierde la comunicacion
// Fecha de correccion: 30/10/24
int ordenescrituraequiposciclico() {
	int res_ordenlecturaequipos;
	bool iniciado = false;
	double menor;
	//cout << "GLOBAL_EJECUTARHILOS: " << GLOBAL_EJECUTARHILOS << endl;
	while (GLOBAL_EJECUTARHILOS) { 
		//cout << "escrituras de tags " << endl;
		{ // Each tag carries its own communication status.

			if (GLOBAL_contador_equipos_mbtcp > 0) {
				ordenescrituraequipos(equipo_mbtcp, GLOBAL_contador_equipos_mbtcp);
			}
			if (GLOBAL_contador_equipos_mbserial > 0) {
				ordenescrituraequipos(equipo_mbserial, GLOBAL_contador_equipos_mbserial);
			}

			//cout << "GLOBAL_contador_equipos_rs232: " << GLOBAL_contador_equipos_rs232 << endl;
			if (GLOBAL_contador_equipos_rs232 > 0) {
				ordenescrituraequipos(equipo_rs232, GLOBAL_contador_equipos_rs232);
			}
		}
		
		menor = funmenor(GLOBAL_periodo_minimo_escritura_tbvalores, GLOBAL_periodo_minimo_escritura_tbdispositivos_sta);
//		cout << "esperar: " << menor << endl;
		auto wake = PeriodicDeadline::Clock::now() + std::chrono::milliseconds(200);
        auto inspect = [&](cls_equipos* tags, int count) {
            for (int i = 0; i < count; ++i) {
                std::lock_guard<std::recursive_mutex> state(*tags[i].state_mutex);
                if (!tags[i].resultado_leer) continue;
                wake = std::min(wake, tags[i].instant_deadline.next());
                if (tags[i].registrable && habilitarescribir(tags[i].fueescritodb, tags[i].solocambios))
                    wake = std::min(wake, tags[i].history_deadline.next());
            }
        };
        inspect(equipo_mbtcp, GLOBAL_contador_equipos_mbtcp);
        inspect(equipo_mbserial, GLOBAL_contador_equipos_mbserial);
        inspect(equipo_rs232, GLOBAL_contador_equipos_rs232);
        std::this_thread::sleep_until(std::max(wake, PeriodicDeadline::Clock::now() + std::chrono::milliseconds(1)));
		iniciado = true;
	}
	if (iniciado) cout << " fin hilo escritura en base de datos local" << endl;
	return 1;
}
