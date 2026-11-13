#ifndef CLS_EQUIPOS

#define CLS_EQUIPOS

#include <stdlib.h>
#include <stdio.h>
#include <iostream>
#include <string.h>
#include <inttypes.h>
#include <time.h>
#include <vector>
#include <memory>
#include <mutex>
#include "periodic_deadline.h"
#include "sample_window.h"
#include <errno.h>
#include <modbus.h>
#include "mysql_connection.h"
#include <cppconn/driver.h>
#include <cppconn/exception.h>
#include <cppconn/resultset.h>
#include <cppconn/statement.h>
#include <cppconn/prepared_statement.h>
#include <curl/curl.h>


#define const_num_equipo 10

using namespace std;

enum GLOBAL_enum_protocolo { MBTCP = 0, MBSERIAL =1, RS232 = 3};

//typedef std::vector<std::string>  StringVector;

class cls_equipos {
public:
	std::shared_ptr<std::recursive_mutex> state_mutex = std::make_shared<std::recursive_mutex>();
	std::shared_ptr<std::mutex> buffer_mutex = std::make_shared<std::mutex>();
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
	int numerodebits;
	int controldeflujo;
	string equipo;
	string usuario;
	string password;
	bool habilitar;
	string protocolo;
	string tag;
	int registro;
	string funcion;
	string interpretacion;
	float escalamiento;
	string rastro;

	uint16_t muestreo;
	time_t ultimo_muestreo; // ultimo muestreo para la lecturas y alarmas
	time_t ultimo_insertar; // ultimo comando insertar en base de datos tbvalores
	time_t ultimo_insertar_ex; // ultimo comando insertar en bd externa
	time_t ultima_escritura_tbinstantaneo;
	bool resultado_leer;

	string valoresmax;
	string valoresmin;
	string activaralarmas;

	string clsvalorstr;
	int clsvalor;
	float clsvalorfloat;
	
	string oldclsvalorstr;
	int oldclsvalor;
	float oldclsvalorfloat;

	bool fuecambiado; // bit dice si hubo cambio respecto ultima lectura
	bool fueescritodb; // bit dice si ya fue escrito en la base de datos

	static string static_clsnombre_mbtcp;
	static int static_clsregistros_mbtcp[20];
	static int static_clsdatosleidos_mbtcp[20];
	static time_t static_clsultimalectura_mbtcp;

	static string static_clsnombre_mbserial;
	static int static_clsregistros_mbserial[20];
	static int static_clsdatosleidos_mbserial[20];
	static time_t static_clsultimalectura_mbserial;


	string usuarioregpool;
	string temp6;
	string temp7;
	string temp8;

	// Nombres heredados del esquema SQL: delimitadores de mensajes RS232.
	string  html_id;
	string  html_final;

	bool preservar_picos = false;
	uint64_t sample_sequence = 0;
	uint64_t change_revision = 0;
	SampleValue latest_sample;
	SampleWindow sample_window;
	PeriodicDeadline read_deadline, instant_deadline, history_deadline, status_deadline;
	double  periodo;
	int 	bit1; // especifica cual bit dentro del word se va usar si es -1 entonces se leen todos
	// los bits para obtener el valor
	int		cantidad1;
	string	comentario;
	string	unidad; // unidad de ingenieria, ej m,psi,kg
	int		solocambios; //si es 0: siempre actualiza o registra valor, si es 1 entonces solo
						// los cambios se actualizan o registran
	int		registrable; // especifica si de debe insertar o no en la base de datos
	static string clsarchivo; // identifica cual fue el archivo anterior abierto
	static time_t clsarchivo_tiempo; // toma la hora en que se abrio el archivo

	int contador_errores;
	int contador_errores_acumulados;

	int* pcontador_errores;
	int* pcontador_errores_acumulados;
	double tiemporeset_cuenta_errores;
	double tiempotranscurridoerrores;
	double* ptiemporeset_cuenta_errores;
	double* ptiempotranscurridoerrores;
	double ultima_llamada_leer;
	double* pultima_llamada_leer;
	int cuenta_errores_maximo;
	string descripcionerror;
	bool estadocomunicacion;
	bool* pestadocomunicacion;

	int resultadoleer_rc = -1;
	// metodos
	cls_equipos();
	int leer();
	std::shared_ptr<unsigned char> buffer_owner; // Shared by tags on one serial device.
	unsigned char* buf;
	int tamanobuffer;
	int tamanoventana;
	int n_buf;
	int* p_n_buf;

private:
	void leer_mbtcp(void);
	void leer_mbserial(void);
	void leer_rs232(void);

	float convert_float_fp(float regVal);
	void esperar(double segundos);
	void incrementarerrores();
	void setvalorfloat(float);
	void setvalorstring(string);


};



#endif
