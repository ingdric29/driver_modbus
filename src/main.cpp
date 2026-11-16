/*
 * Archivo: main.cpp
 * Descripción: Punto de entrada y definiciones globales heredadas.
 * Necesita: Argumentos CLI y configdb.cfg; servicios SQL y bibliotecas del driver.
 * Retornos: EXIT_SUCCESS al finalizar normalmente; EXIT_FAILURE ante fallo de arranque o
 * ejecución.
 */

#include "include/driver_services.h"
#include "include/conversiones.h"
#include "include/version.h"
#include "include/modbus_transport.h"
#include "include/runtime_control.h"
#include "include/monitor_state.h"
#include "include/monitor_server.h"
#include "include/startup_console.h"
#include <cstdlib>
#include <ctime>
#include <iostream>
#include <string>

// Keep globals here and in their original order to preserve initialization.
Logger logger_main;

using namespace std;

// variables Globales

cls_equipos *equipo_mbtcp; // se instancias la clase
cls_equipos *equipo_mbserial;
cls_equipos *equipo_rs232;
cls_equipos *equipo_rs232_distintos;
cls_defalarmas *defalarmas;
cls_cmd *cmd;         // se instancias la clase
cls_email *emaillist; //directorio
cls_defeventos *defeventos;
cls_sms *smslist; // dista de numeros de telefonos

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
int GLOBAL_contador_equipos;  // indica cuantos equipos se estan leyendo
int GLOBAL_contador_comandos; // indica cuantos comandos hay en la tabla de tbcmd
int GLOBAL_contadorext_comandos; // indica cuantos comandos hay en la tabla de tbcmd con orden de ejecutar en base de datos externa
long GLOBAL_errores;
long GLOBAL_errores_ultimo;
time_t GLOBAL_tiempo_actual;
time_t GLOBAL_ultimo_tiempo_tbejecutar_dblocal;
time_t GLOBAL_ultimo_tiempo_tbejecutar_dbexterna;
struct tm *GLOBAL_timeinfo;

string GLOBAL_db_direccion;
string GLOBAL_db_puerto;
string GLOBAL_db_usuario;
string GLOBAL_db_clave;
string GLOBAL_db_nombre;
double GLOBAL_periodo_muestreo_tbejecutar; // se usa para ejecutar los comandos en la tabla tbcmd
double GLOBAL_periodo_muestreo_tbcambios;  // se usa para detectar cambios en la configuracion

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

/**
 * Procesa argumentos, carga configuración e inicia el coordinador.
 * Parámetros: int argc, char *argv[]
 * Retorno: EXIT_SUCCESS o EXIT_FAILURE.
 * Efectos: Instala señales, configura el monitor y libera recursos ante fallo.
 */
int main(int argc, char *argv[]) {
    install_driver_signals();

    if (argc > 1 && std::string(argv[1]) == "--version") {
        cout << "Automonitor Driver " << AUTOMONITOR_VERSION << " | commit " << AUTOMONITOR_COMMIT
             << " | build " << AUTOMONITOR_BUILD_DATE << endl;
        return EXIT_SUCCESS;
    }
    if (argc > 1)
        GLOBAL_habilitar_imprimir = atoi(argv[1]);
    cout << "\n  AUTOMONITOR  /  MODBUS DRIVER\n"
         << "  Version " << AUTOMONITOR_VERSION << "  |  AUTOMATRONI C.A.\n"
         << "  www.automatroni.com\n"
         << "  ------------------------------------------------------------\n";
    startup_row("Commit", AUTOMONITOR_COMMIT);
    startup_row("Compilacion (UTC)", AUTOMONITOR_BUILD_DATE);
    cout << endl;

    if (loadconfigdb() == 1) {
        cout << "\n[ OK ] Configuracion cargada.\n" << endl;
        logger_main.registrarError("INFO: Configuracion del driver cargada.", false);
    } else {
        cout << "[ ERROR ] No se pudo cargar configdb.cfg." << endl;
        logger_main.registrarError("ERROR: No se pudo cargar configdb.cfg.");
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

    string arrcampos[] = {"activas", "reconocida"};
    string arrvalores[] = {"1", "1"};
    //int res_act=0;

    GLOBAL_ultima_revision_tablas_locales = time(NULL);
    GLOBAL_ultimo_reseteo_mysqlexterno = 0;
    GLOBAL_contador_defeventos = -1;

    logger_main.registrarError("INFO: Inicio del driver.", false);
    try {
        monitor::State::instance().configure(GLOBAL_nombre_concentrador,
                                             "tcp://" + stdblocal.direccion + ":" +
                                                 std::to_string(stdblocal.puerto),
                                             stdblocal.nombre);
        monitor::Server monitor_server(obtenerDirectorio(".runtime"));
        return muestrear() < 0 ? EXIT_FAILURE : EXIT_SUCCESS;
    } catch (const std::exception &error) {
        logger_main.registrarError(std::string("Fatal worker failure: ") + error.what());
        ModbusLease::clear();
        liberarmemoria();
        return EXIT_FAILURE;
    }
}
