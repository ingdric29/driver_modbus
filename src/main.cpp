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
#include "include/version.h"
#include "include/modbus_transport.h"
#include "include/runtime_control.h"
#include "include/driver_metrics.h"
#include "include/monitor_state.h"
#include "include/monitor_server.h"
#ifdef _WIN32
#include <Windows.h>
#else
#include <unistd.h>
#endif
#include "include/rs232.h"

#include "include/cls_logger.h"
#include "include/startup_console.h"
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
std::atomic<bool> GLOBAL_stop_requested{false};
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
long long GLOBAL_replicacion_inicial_id = -1;

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


/*
============================================================
MAIN
============================================================
*/

int main(int argc, char* argv[]) {
    install_driver_signals();

    if (argc > 1 && std::string(argv[1]) == "--version") {
        cout << "Automonitor Driver " << AUTOMONITOR_VERSION << " | commit " << AUTOMONITOR_COMMIT
             << " | build " << AUTOMONITOR_BUILD_DATE << endl;
        return EXIT_SUCCESS;
    }
	if (argc > 1) GLOBAL_habilitar_imprimir = atoi(argv[1]);
    cout << "\n  AUTOMONITOR  /  MODBUS DRIVER\n"
         << "  Version " << AUTOMONITOR_VERSION << "  |  AUTOMATRONI C.A.\n"
         << "  www.automatroni.com\n"
         << "  ------------------------------------------------------------\n";
    startup_row("Commit", AUTOMONITOR_COMMIT);
    startup_row("Compilacion (UTC)", AUTOMONITOR_BUILD_DATE);
    cout << endl;

	if (loadconfigdb() == 1) {
		cout << "\n[ OK ] Configuracion cargada.\n" << endl;
		logger_main.registrarError("INFO: Devlog initialized. Configuration file loaded.", false);
	}
	else {
		cout << "[ ERROR ] No se pudo cargar configdb.cfg." << endl;
		logger_main.registrarError("ERROR: Configuration file configdb.cfg failed to load!");
        return EXIT_FAILURE;
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

	logger_main.registrarError("INFO: Start Devlog.", false);
	try {
        monitor::State::instance().configure(GLOBAL_nombre_concentrador,
            "tcp://" + stdblocal.direccion + ":" + std::to_string(stdblocal.puerto), stdblocal.nombre);
        monitor::Server monitor_server(obtenerDirectorio(".runtime"));
        return muestrear() < 0 ? EXIT_FAILURE : EXIT_SUCCESS;
    }
    catch (const std::exception& error) {
        logger_main.registrarError(std::string("Fatal worker failure: ") + error.what());
        ModbusLease::clear(); liberarmemoria(); return EXIT_FAILURE;
    }
};


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
// 1 consiguio etiqueta y -1 no consiguio etiqueta

/*
==================================================
MUESTREO
==================================================
*/

int muestrear(void) {
	GLOBAL_estado_primera_ejecucion_programa = 1;
	cout << "[ INICIO ] Cargando catalogo de dispositivos..." << endl;
	while (!GLOBAL_stop_requested) {
        monitor::State::instance().phase("CARGANDO");
		if (ordenlecturaconfiguracion() < 0) { liberarmemoria(); return -1; }
		if (GLOBAL_stop_requested) { liberarmemoria(); break; }
        cout << "\n  ADQUISICION\n";
        startup_row("Tags Modbus TCP", std::to_string(GLOBAL_contador_equipos_mbtcp));
        startup_row("Tags Modbus RTU", std::to_string(GLOBAL_contador_equipos_mbserial));
        startup_row("Tags RS232", std::to_string(GLOBAL_contador_equipos_rs232));
        startup_row("Puertos RS232", std::to_string(GLOBAL_contador_equipos_rs232_distintos));
        cout << endl;
        GLOBAL_EJECUTARHILOS = true;
		actualizar_valor_condicion(&stdblocal, "tbdispositivos_sta", "estado", "0", "`id`>'0'");
        monitor::State::instance().phase("EJECUTANDO");
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
        cout << "[ OK ] Trabajadores iniciados.\n"
             << "       Monitor: automonitorctl  |  Detener: Ctrl+C\n" << endl;
        workers.join();
        monitor::State::instance().phase(GLOBAL_stop_requested ? "DETENIENDO" : "RECARGANDO");
        ModbusLease::clear();

		cout << "\n[ ESTADO ] Trabajadores detenidos." << endl;
		
		esperar(5);
		liberarmemoria();

	}
    logger_main.registrarError(DriverMetrics::instance().summary(), false);
    cout << DriverMetrics::instance().console_summary() << endl;
    return 0;
}


int ordenejecucuionlocal() {
	std::lock_guard<std::recursive_mutex> commands(GLOBAL_command_mutex);
	int res_cmd_local = 0;
	time_t res_segundos;
	int res_ejecutar = -1;
	string res_lote = "";
	//orden de ejecucion local
	if (leer_tbejecutar(res_cmd_local, res_segundos, res_lote, GLOBAL_db_direccion, getVal(GLOBAL_db_puerto), GLOBAL_db_nombre, GLOBAL_db_usuario, GLOBAL_db_clave) < 0) return -1;
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
						
						if (res_ejecutar == 0) {
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
	int res_cmd_externa = 0;
	time_t res_segundos;
	int res_ejecutar_externa = -1;
	string res_lote = "";
	// orden de jecucion externa desde una base de datos en la nube	  
	if (leer_tbejecutar(res_cmd_externa, res_segundos, res_lote, GLOBAL_dbext_direccion, getVal(GLOBAL_dbext_puerto), GLOBAL_dbext_nombre, GLOBAL_dbext_usuario, GLOBAL_dbext_clave) < 0) return -1;
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
						if (res_ejecutar_externa == 0) {
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
    PeriodicDeadline metrics_deadline;
    metrics_deadline.defer(60);
	while (driver_running()) {
        if (metrics_deadline.due()) {
            logger_main.registrarError(DriverMetrics::instance().summary(), false);
            cout << DriverMetrics::instance().console_summary() << endl;
            metrics_deadline.advance(60);
        }
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
		esperar(std::min(GLOBAL_periodo_muestreo_tbejecutar, GLOBAL_periodoext_muestreo_tbejecutar));
	}
	cout << " fin hilo lectura ordenes de ejecucion " << endl;
	return 1;
}


// lee valores de los equipos indicados y los inserta en las bases de datos locales y externa
//ordenrevisarposiblesalarmasciclico
int ordenrevisarposiblesalarmasciclico() {
	while (driver_running()) {
		double diferencia_tiempoalarmas = 0.0;
		diferencia_tiempoalarmas = difftime(time(NULL), GLOBAL_ultima_revision_alarmas);
		if (diferencia_tiempoalarmas >= GLOBAL_periodo_revision_alarmas) {
			ordenrevisarposiblesalarmas();
		}
		esperar(GLOBAL_periodo_revision_alarmas);
	}
	cout << " fin hilo ordenrevisarposiblesalarmasciclico " << endl;
	return 1;
}

int ordenrevisarposiblesalarmas() {

	imprimir_mensaje("inicio revision tabla de alarmas: " + cast_int_to_string(GLOBAL_contador_defalarmas), negro);
	for (int j = 0; j < GLOBAL_contador_defalarmas; j++) {
        int result = defalarmas[j].evaluarcompleto();
        monitor::State::instance().alarm(defalarmas[j].id, result >= 0, defalarmas[j].activa);
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
		if (actualizar_directorio(&stdblocal) < 0) return -1;
		if (actualizar_smslist(&stdblocal) < 0) return -1;
		if (actualizar_defalarmas(&stdblocal) < 0) return -1;
		if (actualizar_defeventos(&stdblocal) < 0) return -1;
        for (int rtu = 0; rtu < GLOBAL_contador_equipos_mbserial; ++rtu) {
            for (int serial = 0; serial < GLOBAL_contador_equipos_rs232; ++serial) {
                if (equipo_mbserial[rtu].interface1 == equipo_rs232[serial].interface1) {
                    logger_main.registrarError("RTU and direct RS232 cannot share the same serial interface");
                    return -1;
                }
            }
        }
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

        auto& monitor_state = monitor::State::instance();
        monitor_state.begin_catalog();
        auto register_tags = [&](cls_equipos* tags, int count) {
            for (int i = 0; i < count; ++i) {
                auto& t = tags[i];
                monitor::Tag info;
                info.device = t.nombre; info.protocol = t.protocolo;
                info.endpoint = t.protocolo == "MBTCP" ? t.direccion_ip + ":" + std::to_string(t.puerto) : t.interface1;
                info.slave = t.direccion_esclavo;
                // Length prefixes prevent ambiguous names/endpoints from merging devices.
                for (const auto& part : {info.protocol, info.device, info.endpoint, std::to_string(info.slave)})
                    info.device_key += std::to_string(part.size()) + ":" + part;
                info.name = t.tag; info.unit = t.unidad; info.address = t.registro;
                info.period = std::isfinite(t.periodo) && t.periodo > 0 ? t.periodo : 1;
                t.monitor_key = monitor_state.add_tag(info);
            }
        };
        register_tags(equipo_mbtcp, GLOBAL_contador_equipos_mbtcp);
        register_tags(equipo_mbserial, GLOBAL_contador_equipos_mbserial);
        register_tags(equipo_rs232, GLOBAL_contador_equipos_rs232);
        for (int i = 0; i < GLOBAL_contador_defalarmas; ++i) monitor_state.add_alarm(defalarmas[i].id, defalarmas[i].clsdispositivo, defalarmas[i].descripcion);
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

	while (driver_running()) {
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

	while (driver_running()) {
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

	while (driver_running()) {
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

	while (driver_running()) {
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
    if (!cmd) return -1;
	int indice = -1;
	stringstream ss1;
	ss1 << "contador comandos:" << GLOBAL_contador_cmd;
	imprimir_mensaje(ss1.str());
	for (int j = 0; j < GLOBAL_contador_cmd; j++) {
		ss1.str("");
		ss1 << "valor id buscado=" << id << " | leido id=" << cmd[j].id << " | " << cmd[j].nombre << "indice ram=" << j;
		imprimir_mensaje(ss1.str());
		if (id == cmd[j].id) {
			indice = j;
			break;
		}
	}
	return indice;
}


// dado el id, encuentra la posicion en memoria ram. cmd[j]
// es decir encuentra a j, en la expresion cmd[j] = id
int email_buscarid(int id) {
    if (!emaillist) return -1;
	int indice = -1;
	for (int j = 0; j < GLOBAL_contador_directorio; j++) {
		if (id == emaillist[j].id) {
			indice = j;
			break;
		}
	} //for
	return indice;
}


// dado el id, encuentra la posicion en memoria ram. cmd[j]
// es decir encuentra a j, en la expresion cmd[j] = id
int sms_buscarid(int id) {
    if (!smslist) return -1;
	int indice = -1;
	for (int j = 0; j < GLOBAL_contador_numerossms; j++) {
		if (id == smslist[j].clsid) {
			indice = j;
			break;
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

	while (driver_running()) {
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
		modo[1] = equipo_serial->paridad.empty() ? 'N' : (char)toupper(equipo_serial->paridad[0]);
		modo[2] = char(equipo_serial->bitsparada + 48);
		modo[3] = 0;

		velocidadserial = equipo_serial->velocidad;
		controldeflujo = equipo_serial->controldeflujo;
		while (driver_running()) { //while1

			if (RS232_OpenComport(numpuertoserial, velocidadserial, modo, controldeflujo)) // retorna 1 en caso de error
			{
				//falla en abrir puerto;
				imprimir_mensaje("falla en abrir puerto: (" + nombrepuerto + ")", rojo);
				logger_main.registrarError("ERROR: Failed to open port: (" + nombrepuerto + ")");

			}
			else {
				while (driver_running()) {
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
			esperar(10);
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
