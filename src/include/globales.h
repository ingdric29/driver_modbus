/*
 * Archivo: globales.h
 * Descripción: Declara contratos y servicios compartidos entre módulos.
 * Necesita: Definiciones únicas del estado y las implementaciones correspondientes.
 * Retornos: La cabecera no retorna valores por sí sola; los métodos conservan sus contratos
 * documentados en implementación.
 */

#ifndef GLOBALES1

#define GLOBALES1

#include "cls_equipos.h"
#include "cls_cmd.h"
#include "cls_email.h"
#include "cls_sms.h"
#include <time.h>
#include <vector>
#include <thread>
#include <atomic>
#include <mutex>
extern std::recursive_mutex GLOBAL_command_mutex;

extern cls_equipos *equipo_mbtcp;
extern cls_equipos *equipo_mbserial;
extern cls_equipos *equipo_rs232;
extern cls_equipos *equipo_rs232_distintos;
extern cls_cmd *cmd; // se instancias la clase
extern cls_email *emaillist;
extern cls_sms *smslist;

extern int GLOBAL_contador_equipos_mbtcp;
extern int GLOBAL_contador_equipos_mbserial;
extern int GLOBAL_contador_equipos_rs232;
extern int GLOBAL_contador_equipos_rs232_distintos;
extern int GLOBAL_contador_defalarmas;
extern int GLOBAL_contador_defeventos;
extern int GLOBAL_contador_cmd;

extern std::atomic<bool> GLOBAL_EJECUTARHILOS;
extern uint32_t G_modbus_timeout;

extern int GLOBAL_contador_equipos; // indica cuantos equipos se estan leyendo
extern int
    GLOBAL_contador_comandos; // indica cuantos comandos hay en la tabla de tbcmd con orden de ejecutar
extern int
    GLOBAL_contadorext_comandos; // indica cuantos comandos hay en la tabla de tbcmd con orden de ejecutar en base de datos externa
extern int GLOBAL_contador_directorio; // indicas cuantas entradas hay en la tabla directorio
extern int GLOBAL_contador_numerossms;
extern long GLOBAL_errores;
extern long GLOBAL_errores_ultimo;
extern time_t GLOBAL_tiempo_actual;
extern time_t GLOBAL_ultimo_tiempo_tbejecutar_dblocal;
extern time_t GLOBAL_ultimo_tiempo_tbejecutar_dbexterna;
extern struct tm *GLOBAL_timeinfo;

extern string GLOBAL_db_direccion;
extern string GLOBAL_db_puerto;
extern string GLOBAL_db_usuario;
extern string GLOBAL_db_clave;
extern string GLOBAL_db_nombre;
extern double
    GLOBAL_periodo_muestreo_tbejecutar; // se usa para ejecutar los comandos en la tabla tbcmd
extern double GLOBAL_periodo_muestreo_tbcambios; // se usa para detectar cambios en la configuracion
extern double GLOBAL_periodo_revision_alarmas;
extern double GLOBAL_ultima_revision_alarmas;

extern string GLOBAL_dbext_direccion;
extern string GLOBAL_dbext_puerto;
extern string GLOBAL_dbext_usuario;
extern string GLOBAL_dbext_clave;
extern string GLOBAL_dbext_nombre;
extern double GLOBAL_periodoext_muestreo_tbejecutar;

extern string GLOBAL_nombre_concentrador;
extern string GLOBAL_picos_tags;
extern long long GLOBAL_replicacion_inicial_id;

extern double GLOBAL_periodo_minimo_escritura_tbvalores;
extern double GLOBAL_periodo_minimo_escritura_tbvalores_ex;
extern double GLOBAL_ultimotiempo_escritura_tbvalores;
extern double GLOBAL_periodo_revision_tablas_locales;
extern double GLOBAL_periodo_revision_tablas_externas;
extern double GLOBAL_ultima_revision_tablas_locales;
extern double GLOBAL_ultima_revision_tablas_externas;

extern double GLOBAL_maximo_dias_antiguedad_tbvalores;
extern double GLOBAL_maximo_dias_antiguedad_tbinstantaneos;
extern double GLOBAL_maximo_dias_antiguedad_tbvalores_ext;
extern double GLOBAL_maximo_dias_antiguedad_tbinstantaneos_ext;

extern double GLOBAL_sicronizacion_tiempo;
extern double GLOBAL_sicronizacion_periodo;

extern int GLOBAL_cantidadregistrossubir;

extern int GLOBAL_cont_cambio_tablacom;
extern time_t GLOBAL_tbcambio_tiempo_ultimochequeo;

extern string separadorigual;
extern string separadorguion;
extern double G_diferencia;

//extern char* payload_text[13];
extern std::atomic<bool> GLOBAL_ACCESO_EXTERNO;
extern int GLOBAL_habilitar_acceso_externo; //0=no,1=si

enum GLOBAL_colores {
    nada = 0,
    negro = 30,
    rojo = 31,
    verde = 32,
    amarillo = 33,
    azul = 34,
    magenta = 35,
    cian = 36,
    blanco = 37
};

extern string GLOBAL_direccionip_dns;

extern double GLOBAL_periodo_reseteo_mysqlexterno;
extern double GLOBAL_ultimo_reseteo_mysqlexterno;

int loadconfigdb();
int insertardb(time_t fechatiempo, string tag, string valorstr, float valorfloat, int dispositivo,
               string rastro, string nombre_concentrador, string nombre_dispositivo,
               string direccion_db, int puerto_db, string nombre_db, string usuario_db,
               string clave_db, string dbtabla);
//  leer_tbejecutar, esta funcion se encarga de verificar si hay una orden para leer ejecutar las instrucciones
//  ubicadas en la tabla tbcmd
int leer_tbejecutar(int &res_ejecutar, time_t &segundos, string &lote, string direccion_db,
                    int puerto_db, string nombre_db, string usuario_db, string clave_db);
//  leer_tbcmd lee los comandos a ejecutar y los almacena en un array en memoria RAM
int leer_tbcmd(string direccion_db, int puerto_db, string nombre_db, string usuario_db,
               string clave_db);
int escribir_tbcmd_fechahora(int id, string direccion_db, int puerto_db, string nombre_db,
                             string usuario_db, string clave_db);
// lee la tabla tbcambios
int lee_tbcambios(void); //1 detecto cambios
int actualizar_leer_equipo(GLOBAL_enum_protocolo protocoloenum);
int ordenlecturaequipos(cls_equipos *equipo, int arge_cantidad);
int ordenescrituraequipos(cls_equipos *equipo, int arge_cantidad);
int ordenescrituraequiposciclico();
int ordenejecucuionlocal();
int ordenejecucuionexterna();
int ordenlecturaconfiguracion();
int ordenlecturaequiposciclico(cls_equipos *equipo, int arge_cantidad);
int ordenejecucionciclico();
int ordenlecturaconfiguracionciclico();
int borrar_registros(double dias, string direccion_db, int puerto_db, string nombre_db,
                     string usuario_db, string clave_db, string dbtabla);
int ordenborrarregistroslocales();
int ordenborrarregistrosexternos();
int ordenborrarregistrosexternosciclico();
int ordenborrarregistroslocalesciclico();
int ordenrevisarposiblesalarmasciclico();
int ordenrevisarposiblesalarmas();
int sincronizar_tablas(struct stdatabase *dblocal, struct stdatabase *dbexterna, string nombre,
                       int GLOBAL_cantidadregistrossubir);
int orden_sincronizar_tablas_ciclico();
void imprimir_mensaje(string str, GLOBAL_colores color = nada);
void imprimir_mensaje_importante(string str, GLOBAL_colores = nada);
std::vector<string> split1(const string &cadena, const char &delim);
int muestrear(void);
int getVal(string);
float getValfloat(string);
void esperar(double segundos);

int actualizar_defalarmas(struct stdatabase *db);
int actualizar_defeventos(struct stdatabase *db);
int actualizar_posiciones_clsdefeventos();
int actualizar_smslist(struct stdatabase *db);

int enviar_email(struct storigen origen, struct stdestino destino);

// dado el id, encuentra la posicion en memoria ram. cmd[j]
// es decir encuentra a j, en la expresion cmd[j] = id
int cmd_buscarid(int id);
int email_buscarid(int id);
int sms_buscarid(int id);

int actualizar_directorio(struct stdatabase *db);

int actualizar_valor(struct stdatabase *db, string tabla, int indice, string *campo, string *valor,
                     int cantidad);
int actualizar_valor3(struct stdatabase *db, string tabla, int indice1, int indice2, int indice3,
                      string *campo, string *valor, int cantidad);

int insertar_valor(struct stdatabase *db, string tabla, string *campo, string *valor, int cantidad);

string exec(const char *cmd1);

bool chequear_internet(string ipaddress1 = "1.0.0.1");

int buscarentabla(struct stdatabase *db, string tabla, string campo, string valor);
int buscarentabla3(struct stdatabase *db, string tabla, string campo1, string campo2, string campo3,
                   string valor1, string valor2, string valor3);

int insertar_inteligente(struct stdatabase *db, string tabla, int indice, string *campo,
                         string *valor, int cantidad);
int insertar_inteligente3(struct stdatabase *db, string tabla, int indice1, int indice2,
                          int indice3, string *campo, string *valor, int cantidad);

int replicaciontablaselectiva(struct stdatabase *dborigen, string tablaorigen,
                              struct stdatabase *dbdestino, string tabladestino, string *campos,
                              int cantidadcampos, int limitefilastablaorigen);

int actualizar_valor_condicion(struct stdatabase *db, string tabla, string campo, string valor,
                               string condicion);

int actualizar_posiciones_clsdefeventos();

int orden_actualizarvalores_clsdefeventos();

void liberarmemoria(void);

void lectura_puerto_serial(cls_equipos *equipo_serial);

double funmenor(double v1, double v2);

extern double GLOBAL_periodo_minimo_escritura_tbdispositivos_sta;

extern double GLOBAL_tiempo_ultimaescritura_tbdispositivos_sta;

extern std::atomic<int> GLOBAL_estado_primera_ejecucion_programa;

extern double GLOBAL_periodo_revision_eventos;
extern double GLOBAL_tiempo_ultima_revision_eventos;

extern double GLOBAL_maxima_cuenta_errores;
extern double GLOBAL_tiempo_restablecimiento_comunicacion;

#endif
