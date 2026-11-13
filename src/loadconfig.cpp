/*
programa dedarrollado por ing. Alejandro diomede - Automatroni - email: diomede3009@gmail.com
cargar en memoria el archivo de configuracion
*/
#include "include/globales.h"
#include <iostream>
#include "include/INIReader.h"
#include "include/cls_equipos.h"
#include "include/cls_cmd.h"
#include "include/conversiones.h"
#include "include/cls_defalarmas.h"
#include "include/cls_defeventos.h"
#include "include/estructuras.h"
#include "mysql_connection.h"
#include <cppconn/driver.h>
#include <cppconn/exception.h>
#include <cppconn/resultset.h>
#include <cppconn/statement.h>
#include <cppconn/prepared_statement.h>

#include <chrono>
#include <memory>
#include <thread>

#include "include/cls_logger.h"

Logger logger_loadconfig;

#define timeout_mysql 3


using namespace std;

/* ESTAS VARIABLES ESTAN DEFINIDAS EN EL ARCHIVO main.cpp*/

extern cls_defalarmas* defalarmas;
extern cls_defeventos* defeventos;

extern cls_email* emaillist; //directorio lista de correos
extern cls_sms* smslist;

extern stdatabase stdblocal;
extern stdatabase stdbexterna;

extern storigen GLOBAL_email_origen;
/*
========================================================
INICIA LA LECTURA DE LA CONFIGURACION INICIAL
========================================================
*/

int loadconfigdb()
{
	int contador_campos = 0;
	INIReader reader(obtenerDirectorio("configdb.cfg"));

	if (reader.ParseError() < 0) {
		std::cout << "No se pudo cargar 'configdb.ini'\n";
		return -1;
	}
	std::cout << "leyendo archivo 'configdb.cfg'" << "\n";
	std::set<std::string> sections = reader.GetSections();
	for (std::set<std::string>::iterator sectionsIt = sections.begin();
		sectionsIt != sections.end();
		sectionsIt++)
	{
		//std::cout << "  [" << *sectionsIt << "]: " << endl;
		std::set<std::string> fields = reader.GetFields(*sectionsIt);
		contador_campos = 0;
		GLOBAL_db_direccion = reader.Get(*sectionsIt, "db_direccion", "127.0.0.1");
		GLOBAL_db_nombre = reader.Get(*sectionsIt, "db_nombre", "dbdevlog");
		GLOBAL_db_puerto = reader.Get(*sectionsIt, "db_puerto", "3306");
		GLOBAL_db_usuario = reader.Get(*sectionsIt, "db_usuario", "sigge");
		GLOBAL_db_clave = reader.Get(*sectionsIt, "db_clave", "master3112");
		GLOBAL_periodo_muestreo_tbejecutar = cast_string_to_double(reader.Get(*sectionsIt, "periodo_muestreo_tbejecutar", "10"));
		GLOBAL_periodo_muestreo_tbcambios = cast_string_to_double(reader.Get(*sectionsIt, "periodo_muestreo_tbcambios", "10"));

		GLOBAL_dbext_direccion = reader.Get(*sectionsIt, "dbext_direccion", "127.0.0.1");
		GLOBAL_dbext_nombre = reader.Get(*sectionsIt, "dbext_nombre", "dbdevlog");
		GLOBAL_dbext_puerto = reader.Get(*sectionsIt, "dbext_puerto", "3306");
		GLOBAL_dbext_usuario = reader.Get(*sectionsIt, "dbext_usuario", "sigge");
		GLOBAL_dbext_clave = reader.Get(*sectionsIt, "dbext_clave", "master3112");
		GLOBAL_periodoext_muestreo_tbejecutar = cast_string_to_double(reader.Get(*sectionsIt, "periodoext_muestreo_tbejecutar", "5"));

		GLOBAL_nombre_concentrador = reader.Get(*sectionsIt, "nombre_concentrador", "raspi");

		GLOBAL_periodo_minimo_escritura_tbvalores = cast_string_to_double(reader.Get(*sectionsIt, "periodo_minimo_escritura_tbvalores", "30"));
		GLOBAL_periodo_minimo_escritura_tbvalores_ex = cast_string_to_double(reader.Get(*sectionsIt, "periodo_minimo_escritura_tbvalores_ext", "30"));

		GLOBAL_periodo_revision_tablas_locales = cast_string_to_double(reader.Get(*sectionsIt, "periodo_revision_tablas_locales", "300"));
		GLOBAL_periodo_revision_tablas_externas = cast_string_to_double(reader.Get(*sectionsIt, "periodo_revision_tablas_externas", "300"));

		GLOBAL_maximo_dias_antiguedad_tbvalores = cast_string_to_double(reader.Get(*sectionsIt, "maximo_dias_antiguedad_tbvalores", "30"));
		GLOBAL_maximo_dias_antiguedad_tbinstantaneos = cast_string_to_double(reader.Get(*sectionsIt, "maximo_dias_antiguedad_tbinstantaneos", "0.5"));

		GLOBAL_maximo_dias_antiguedad_tbvalores_ext = cast_string_to_double(reader.Get(*sectionsIt, "maximo_dias_antiguedad_tbvalores_ext", "30"));
		GLOBAL_maximo_dias_antiguedad_tbinstantaneos_ext = cast_string_to_double(reader.Get(*sectionsIt, "maximo_dias_antiguedad_tbinstantaneos_ext", "0.5"));

		GLOBAL_sicronizacion_periodo = cast_string_to_double(reader.Get(*sectionsIt, "sicronizacion_periodo", "300"));

		GLOBAL_periodo_revision_alarmas = cast_string_to_double(reader.Get(*sectionsIt, "periodo_revision_alarmas", "60"));


		GLOBAL_email_origen.from = reader.Get(*sectionsIt, "smtp_from", "automonitor@cd-solec.com");
		GLOBAL_email_origen.username = reader.Get(*sectionsIt, "smtp_usuario", "automonitor@cd-solec.com");
		GLOBAL_email_origen.password = reader.Get(*sectionsIt, "smtp_pass", "automa2424.e");
		GLOBAL_email_origen.smtp = reader.Get(*sectionsIt, "smtp_servidor", "smtps://cd-solec.com");

		GLOBAL_direccionip_dns = reader.Get(*sectionsIt, "direccionip_dns", "1.0.0.1");

		GLOBAL_periodo_reseteo_mysqlexterno = cast_string_to_double(reader.Get(*sectionsIt, "periodo_reseteo_mysqlexterno", "600"));

		GLOBAL_habilitar_acceso_externo = cast_string_to_int(reader.Get(*sectionsIt, "habilitar_acceso_externo", "1"));

		GLOBAL_periodo_minimo_escritura_tbdispositivos_sta = cast_string_to_double(reader.Get(*sectionsIt, "GLOBAL_periodo_minimo_escritura_tbdispositivos_sta", "5"));

		GLOBAL_periodo_revision_eventos = cast_string_to_double(reader.Get(*sectionsIt, "GLOBAL_periodo_revision_eventos", "4"));

		GLOBAL_maxima_cuenta_errores = cast_string_to_double(reader.Get(*sectionsIt, "maxima_cuenta_errores", "50"));

		GLOBAL_tiempo_restablecimiento_comunicacion = cast_string_to_double(reader.Get(*sectionsIt, "tiempo_restablecimiento_comunicacion", "60"));

		GLOBAL_cantidadregistrossubir = cast_string_to_int(reader.Get(*sectionsIt, "cantidadregistrossubir", "500"));
		
		GLOBAL_clave1 = reader.Get(*sectionsIt, "clave1", "0");
		GLOBAL_clave2 = reader.Get(*sectionsIt, "clave2", "0");

		for (std::set<std::string>::iterator fieldsIt = fields.begin();
			fieldsIt != fields.end();
			fieldsIt++)
		{
			if (fieldsIt != fields.begin())
				//    std::cout << ", ";
				//std::cout << *fieldsIt << "=" << reader.Get(*sectionsIt, *fieldsIt, "xxx");
				contador_campos++;
		}
		std::cout << std::endl;
		std::cout << "nombre del concentrador      : " << GLOBAL_nombre_concentrador << endl;
		std::cout << "Direccion base de datos local: " << GLOBAL_db_direccion << endl;
		std::cout << "Nombre base de datos         : " << GLOBAL_db_nombre << endl;
		std::cout << "Puerto base de datos         : " << GLOBAL_db_puerto << endl;
		std::cout << "Usuario base de datos        : " << GLOBAL_db_usuario << endl;
		
		std::cout << separadorguion << endl;
		std::cout << "Direccion base de datos externo: " << GLOBAL_dbext_direccion << endl;
		std::cout << "Nombre base de datos           : " << GLOBAL_dbext_nombre << endl;
		std::cout << "Puerto base de datos           : " << GLOBAL_dbext_puerto << endl;
		std::cout << "Usuario base de datos          : " << GLOBAL_dbext_usuario << endl;
		std::cout << "Habilitar acceso externo       : " << GLOBAL_habilitar_acceso_externo << endl;
		
		std::cout << "Periodo sincronizacion local>>externo   : " << GLOBAL_sicronizacion_periodo << endl;
		std::cout << "Cantidad de registros a subir >> externo: " << GLOBAL_cantidadregistrossubir << endl;

		std::cout << "Periodo muestreo tbejecutar local       :   " << GLOBAL_periodo_muestreo_tbejecutar << endl;
		std::cout << "Periodo muestreo tbejecutar externo     : " << GLOBAL_periodoext_muestreo_tbejecutar << endl;
		std::cout << "Periodo muestreo tbcambios              : " << GLOBAL_periodo_muestreo_tbcambios << endl;
		std::cout << "Periodo minimo escritura tbvalores      : " << GLOBAL_periodo_minimo_escritura_tbvalores << endl;
	}

	return 1;
}



// lee la tabla tbcambios
int lee_tbcambios(void) {

	int id = 0;
	int cont_tbregistros;
	int cont_tbdispositivos;
	int cont_tbdefequipos;
	int cont_cambio_tablacom;
	int cont_tbdefalarmas;
	int cambio = 0;
	string direccionpuerto;
	direccionpuerto = "tcp://" + GLOBAL_db_direccion + ":" + GLOBAL_db_puerto;
	GLOBAL_contador_equipos = 0;

	try {
		sql::Driver* driver;
		sql::Connection* con;
		sql::Statement* stmt;
		sql::ResultSet* res;

		/* Crea una conexion */
		driver = get_driver_instance();
		con = driver->connect(direccionpuerto.c_str(), GLOBAL_db_usuario.c_str(), GLOBAL_db_clave.c_str()); //GLOBAL_usuario_db.c_str(), GLOBAL_clave_db.c_str());
		int timeout1 = timeout_mysql;
		con->setClientOption("MYSQL_OPT_CONNECT_TIMEOUT", &timeout1);
		/* Conecta con la base de datos */
		con->setSchema(GLOBAL_db_nombre.c_str());

		stmt = con->createStatement();
		res = stmt->executeQuery("SELECT * FROM `tbcambios` WHERE `id`=1");

		while (res->next()) {
			id = res->getInt("id"); //
			cont_tbregistros = res->getInt("cont_tbregistros");
			cont_tbdispositivos = res->getInt("cont_tbdispositivos");
			cont_tbdefequipos = res->getInt("cont_tbdefequipos");
			cont_tbdefalarmas = res->getInt("cont_tbdefalarmas");
			cont_cambio_tablacom = cont_tbregistros + cont_tbdispositivos + cont_tbdefequipos + cont_tbdefalarmas;

		}

		if (GLOBAL_cont_cambio_tablacom == cont_cambio_tablacom) {
			cambio = 0;
		}
		else {
			cambio = 1;
			GLOBAL_cont_cambio_tablacom = cont_cambio_tablacom;
		}
		con->close();
		delete res;
		delete stmt;
		delete con;
		return cambio;

	}
	catch (sql::SQLException& e) {
		GLOBAL_ACCESO_EXTERNO = false;
		imprimir_mensaje("falla en mysql: ", rojo);
		imprimir_mensaje("se deshabilito lectura mysql externo por " + cast_int_to_string(GLOBAL_periodo_reseteo_mysqlexterno) + " seg ", amarillo);
    	
		std::string mensajeError = "ERROR: SQLException in " + std::string(__FILE__) +
                               "(" + std::string(__FUNCTION__) + ") on line " + std::to_string(__LINE__) +
                               ".# Error description: " + e.what() +
                               " (MySQL error code: " + std::to_string(e.getErrorCode()) +
                               ", SQLState: " + e.getSQLState() + " )";

    	std::cout << mensajeError << std::endl;
    	logger_loadconfig.registrarError(mensajeError);
		return -1;
	}


}


// LA BASE DE DATOS LA LLEVA A MEMORIA RAM
int actualizar_leer_equipo(GLOBAL_enum_protocolo protocoloenum)
{

	int id = 0;
	int ind = 0;
	int cuenta = 0;
	int i2 = 0;
	string cadena;
	string direccionpuerto;
	direccionpuerto = "tcp://" + GLOBAL_db_direccion + ":" + GLOBAL_db_puerto;
	GLOBAL_contador_equipos = 0;
	string query;

	switch (protocoloenum) {
	case MBTCP: query = "SELECT * FROM `qrytablacom` WHERE `habilitar`=TRUE AND `protocolo`= 'MBTCP' ORDER BY `id` ASC"; break;
	case MBSERIAL: query = "SELECT * FROM `qrytablacom` WHERE `habilitar`=TRUE AND `protocolo`= 'MBSERIAL' ORDER BY `id` ASC"; break;
	case RS232: query = "SELECT * FROM `qrytablacom` WHERE `habilitar`=TRUE AND `protocolo`= 'RS232' ORDER BY `id` ASC"; break;
	}

	//cout << query << endl;

	try {
		sql::Driver* driver;
		sql::Connection* con;
		sql::Statement* stmt;
		sql::ResultSet* res;

		/* Crea una conexion */
		driver = get_driver_instance();
		con = driver->connect(direccionpuerto.c_str(), GLOBAL_db_usuario.c_str(), GLOBAL_db_clave.c_str()); //GLOBAL_usuario_db.c_str(), GLOBAL_clave_db.c_str());
		int timeout1 = timeout_mysql;
		con->setClientOption("MYSQL_OPT_CONNECT_TIMEOUT", &timeout1);
		/* Conecta con la base de datos */
		con->setSchema(GLOBAL_db_nombre.c_str());


		stmt = con->createStatement();
		res = stmt->executeQuery(query.c_str());

		while (res->next() && (ind < 512)) { cuenta = cuenta + 1; ind++; }



		switch (protocoloenum) {
		case MBTCP: 	GLOBAL_contador_equipos_mbtcp = cuenta; cout << "numero tags mbtcp:" << cuenta << endl; break;
		case MBSERIAL: 	GLOBAL_contador_equipos_mbserial = cuenta; cout << "numero tags mbserial:" << cuenta << endl; break;
		case RS232:		GLOBAL_contador_equipos_rs232 = cuenta;
						GLOBAL_contador_equipos_rs232_distintos = 0; //mas adelante se completa
						cout << "numero tags rs232:" << cuenta << endl; break;
		}

		/* asignacion de memoria RAM*/


		switch (protocoloenum) {
		case MBTCP: 	equipo_mbtcp = new cls_equipos[GLOBAL_contador_equipos_mbtcp + 2]; break;
		case MBSERIAL: 	equipo_mbserial = new cls_equipos[GLOBAL_contador_equipos_mbserial + 2]; break;
		case RS232: 	equipo_rs232 = new cls_equipos[GLOBAL_contador_equipos_rs232 + 2]; 
						equipo_rs232_distintos = new cls_equipos[GLOBAL_contador_equipos_rs232 + 2];
						break;
		}

		res = stmt->executeQuery(query.c_str());

		switch (protocoloenum) {

		case MBTCP:
			ind = 0;
			equipo_mbtcp[0].protocolo = "MBTCP";
			while (res->next() && (ind < 512)) {
				id = res->getInt("id"); //
				equipo_mbtcp[ind].id = id;
				equipo_mbtcp[ind].nombre = res->getString("nombre"); //
				equipo_mbtcp[ind].direccion_ip = res->getString("direccion_ip"); //
				equipo_mbtcp[ind].puerto = res->getInt("puerto"); //
				equipo_mbtcp[ind].direccion_esclavo = res->getInt("direccion_esclavo"); //
				equipo_mbtcp[ind].interface1 = res->getString("interface"); //
				equipo_mbtcp[ind].velocidad = res->getInt("velocidad"); //
				equipo_mbtcp[ind].paridad = res->getString("paridad"); //
				equipo_mbtcp[ind].bitsparada = res->getInt("bitsparada"); //
				equipo_mbtcp[ind].equipo = res->getInt("equipo"); //
				equipo_mbtcp[ind].usuario = res->getString("usuario"); //
				equipo_mbtcp[ind].password = res->getString("password"); //
				equipo_mbtcp[ind].habilitar = ((res->getInt("habilitar")) != 0); //
				equipo_mbtcp[ind].tag = res->getString("tag"); //
				equipo_mbtcp[ind].protocolo = res->getString("protocolo"); //
				equipo_mbtcp[ind].registro = res->getInt("registro"); //
				equipo_mbtcp[ind].funcion = res->getString("funcion"); //
				equipo_mbtcp[ind].interpretacion = res->getString("interpretacion"); //
				cadena = res->getString("escalamiento"); //
				equipo_mbtcp[ind].escalamiento = getValfloat(cadena);


				equipo_mbtcp[ind].periodo = getValfloat(res->getString("periodo")); //
				equipo_mbtcp[ind].bit1 = res->getInt("bit1"); //
				equipo_mbtcp[ind].cantidad1 = res->getInt("cantidad1"); //
				equipo_mbtcp[ind].comentario = res->getString("comentario"); //
				equipo_mbtcp[ind].unidad = res->getString("unidad"); //
				equipo_mbtcp[ind].solocambios = res->getInt("solocambios"); //
				equipo_mbtcp[ind].registrable = res->getInt("registrable"); //

				ind++;

			}
			break;

		case MBSERIAL:
			ind = 0;
			equipo_mbserial[0].protocolo = "MBSERIAL";
			while (res->next() && (ind < 512)) {
				id = res->getInt("id"); //
				equipo_mbserial[ind].id = id;
				cadena = res->getString("nombre");
				equipo_mbserial[ind].nombre = cadena; //
				//equipo_mbserial[ind].direccion_ip = res->getString("direccion_ip"); //
				//equipo_mbserial[ind].puerto = res->getInt("puerto"); //
				equipo_mbserial[ind].direccion_esclavo = res->getInt("direccion_esclavo"); //
				equipo_mbserial[ind].interface1 = res->getString("interface"); //
				equipo_mbserial[ind].velocidad = res->getInt("velocidad"); //
				equipo_mbserial[ind].paridad = res->getString("paridad"); //
				equipo_mbserial[ind].bitsparada = res->getInt("bitsparada"); //
				equipo_mbserial[ind].equipo = res->getInt("equipo"); //
				equipo_mbserial[ind].usuario = res->getString("usuario"); //
				equipo_mbserial[ind].password = res->getString("password"); //
				equipo_mbserial[ind].habilitar = ((res->getInt("habilitar")) != 0); //
				equipo_mbserial[ind].tag = res->getString("tag"); //
				equipo_mbserial[ind].protocolo = res->getString("protocolo"); //
				equipo_mbserial[ind].registro = res->getInt("registro"); //
				equipo_mbserial[ind].funcion = res->getString("funcion"); //
				equipo_mbserial[ind].interpretacion = res->getString("interpretacion"); //
				cadena = res->getString("escalamiento"); //
				equipo_mbserial[ind].escalamiento = getValfloat(cadena);
				equipo_mbserial[ind].periodo = getValfloat(res->getString("periodo")); //
				equipo_mbserial[ind].bit1 = res->getInt("bit1"); //
				equipo_mbserial[ind].cantidad1 = res->getInt("cantidad1"); //
				equipo_mbserial[ind].comentario = res->getString("comentario"); //
				equipo_mbserial[ind].unidad = res->getString("unidad"); //
				equipo_mbserial[ind].solocambios = res->getInt("solocambios"); //
				equipo_mbserial[ind].registrable = res->getInt("registrable"); //
				ind++;
			}
			break;

		case RS232:
			ind = 0;
			equipo_rs232[0].protocolo = "RS232";
			while (res->next() && (ind < 512)) {
				id = res->getInt("id"); //
				equipo_rs232[ind].id = id;
				cadena = res->getString("nombre");
				equipo_rs232[ind].nombre = cadena; //
				equipo_rs232[ind].interface1 = res->getString("interface"); //
				equipo_rs232[ind].velocidad = res->getInt("velocidad"); //
				equipo_rs232[ind].paridad = res->getString("paridad"); //
				equipo_rs232[ind].bitsparada = res->getInt("bitsparada"); //
				equipo_rs232[ind].equipo = res->getInt("equipo"); //
				equipo_rs232[ind].habilitar = ((res->getInt("habilitar")) != 0); //
				equipo_rs232[ind].tag = res->getString("tag"); //
				equipo_rs232[ind].protocolo = res->getString("protocolo"); //
				equipo_rs232[ind].registro = res->getInt("registro"); //
				equipo_rs232[ind].funcion = res->getString("funcion"); //
				equipo_rs232[ind].interpretacion = res->getString("interpretacion"); //
				cadena = res->getString("escalamiento"); //
				equipo_rs232[ind].escalamiento = getValfloat(cadena);
				equipo_rs232[ind].html_id = tratamientocadena(res->getString("html_id")); // delimitador inicial
				equipo_rs232[ind].html_final = tratamientocadena(res->getString("html_final")); // delimitador final
				equipo_rs232[ind].periodo = getValfloat(res->getString("periodo")); //
				equipo_rs232[ind].bit1 = res->getInt("bit1"); //
				equipo_rs232[ind].cantidad1 = res->getInt("cantidad1"); //
				equipo_rs232[ind].comentario = res->getString("comentario"); //
				equipo_rs232[ind].unidad = res->getString("unidad"); //
				equipo_rs232[ind].solocambios = res->getInt("solocambios"); //
				equipo_rs232[ind].registrable = res->getInt("registrable"); //
				ind++;
			}
			break;

		}



		int cantidaddispo = ind;
		switch (protocoloenum) {
		case MBTCP:
			for (int i = 0; i < cantidaddispo; i++) {
				int j;
				for (j = 0; j < i; j++) {
					if (equipo_mbtcp[i].nombre.compare(equipo_mbtcp[j].nombre) == 0) {
						equipo_mbtcp[i].pcontador_errores = equipo_mbtcp[j].pcontador_errores;
						equipo_mbtcp[i].pcontador_errores_acumulados = equipo_mbtcp[j].pcontador_errores_acumulados;
						equipo_mbtcp[i].ptiempotranscurridoerrores = equipo_mbtcp[j].ptiempotranscurridoerrores;
						equipo_mbtcp[i].ptiemporeset_cuenta_errores = equipo_mbtcp[j].ptiemporeset_cuenta_errores;
						equipo_mbtcp[i].pultima_llamada_leer = equipo_mbtcp[j].pultima_llamada_leer;
						break;
					}
				}
				if (i == j) {
					//solo se ejecuta uno por cada dispositivo
				}
			}
			break;
		case MBSERIAL:
			for (int i = 0; i < cantidaddispo; i++) {
				int j;
				for (j = 0; j < i; j++) {
					if (equipo_mbserial[i].nombre.compare(equipo_mbserial[j].nombre) == 0) {
						equipo_mbserial[i].pcontador_errores = equipo_mbserial[j].pcontador_errores;
						equipo_mbserial[i].pcontador_errores_acumulados = equipo_mbserial[j].pcontador_errores_acumulados;
						equipo_mbserial[i].ptiempotranscurridoerrores = equipo_mbserial[j].ptiempotranscurridoerrores;
						equipo_mbserial[i].ptiemporeset_cuenta_errores = equipo_mbserial[j].ptiemporeset_cuenta_errores;
						equipo_mbserial[i].pultima_llamada_leer = equipo_mbserial[j].pultima_llamada_leer;
						break;
					}
				}
				if (i == j) {
					//solo se ejecuta uno por cada dispositivo
				}
			}
			break;
		case RS232:
			for (int i = 0; i < cantidaddispo; i++) {
				int j;
				for (j = 0; j < i; j++) {
					if (equipo_rs232[i].nombre.compare(equipo_rs232[j].nombre) == 0) {
						equipo_rs232[i].pcontador_errores = equipo_rs232[j].pcontador_errores;
						equipo_rs232[i].pcontador_errores_acumulados = equipo_rs232[j].pcontador_errores_acumulados;
						equipo_rs232[i].ptiempotranscurridoerrores = equipo_rs232[j].ptiempotranscurridoerrores;
						equipo_rs232[i].ptiemporeset_cuenta_errores = equipo_rs232[j].ptiemporeset_cuenta_errores;
						equipo_rs232[i].pultima_llamada_leer = equipo_rs232[j].pultima_llamada_leer;
						equipo_rs232[i].buffer_owner = equipo_rs232[j].buffer_owner;
						equipo_rs232[i].buf = equipo_rs232[j].buf;
						equipo_rs232[i].p_n_buf = equipo_rs232[j].p_n_buf;
						break;
					}
				}
				if (i == j) {
					//solo se ejecuta uno por cada dispositivo
					GLOBAL_contador_equipos_rs232_distintos++;
					equipo_rs232_distintos[i2] = equipo_rs232[i];
					i2++;
				}
			}
			cout << "numero dispositivos rs232:" << GLOBAL_contador_equipos_rs232_distintos << endl;
			hilosseriales = new std::thread [GLOBAL_contador_equipos_rs232_distintos + 1];
			break;
		}
		delete res;
		delete stmt;
		delete con;
		return 1;

	}
	catch (sql::SQLException& e) {
		GLOBAL_ACCESO_EXTERNO = false;
		imprimir_mensaje("falla en mysql: ", rojo);
		imprimir_mensaje("se deshabilito lectura mysql externo por " + cast_int_to_string(GLOBAL_periodo_reseteo_mysqlexterno) + " seg ", amarillo);
		
		std::string mensajeError = "ERROR: SQLException in " + std::string(__FILE__) +
                               "(" + std::string(__FUNCTION__) + ") on line " + std::to_string(__LINE__) +
                               ".# Error description: " + e.what() +
                               " (MySQL error code: " + std::to_string(e.getErrorCode()) +
                               ", SQLState: " + e.getSQLState() + " )";

    	std::cout << mensajeError << std::endl;
    	logger_loadconfig.registrarError(mensajeError);
		return -1;
	}


}




int insertardb(time_t fechatiempo, string tag, string valorstr, float valorfloat, int dispositivo, string rastro, string nombre_concentrador, string nombre_dispositivo, string direccion_db, int puerto_db, string nombre_db, string usuario_db, string clave_db, string dbtabla)
{

	if (direccion_db.compare(GLOBAL_dbext_direccion) == 0) {
		if (GLOBAL_habilitar_acceso_externo == 0) {
			imprimir_mensaje("en la configuracion configdb esta desactivado el acceso a internet", negro);
			return 0;
		}

		if (GLOBAL_ACCESO_EXTERNO == false) {
			imprimir_mensaje("se desactivo momentaneamente el acceso al servidor mysql externo", negro);
			return 0;
		}
		else {
			if (chequear_internet(GLOBAL_direccionip_dns)) {
				;
			}
			else {
				imprimir_mensaje("no hay internet, no se puede alcanzar el servidor externo", negro);
				return 0;
			}
		}

	}

	try {
		sql::Driver* driver;
		sql::Connection* con;
		sql::Statement* stmt;
		//  sql::ResultSet *res;
		string SQL_STR = "";
		char arrSQL[1024] = { 0 };
		memset(arrSQL, 0, 1024);
		bool resultado = false;
		string direccionpuerto;
		string puerto_db_str = cast_int_to_string(puerto_db);
		string fechatiemporecibido = cast_time_to_datetime(fechatiempo);
		direccionpuerto = "tcp://" + direccion_db + ":" + puerto_db_str;
		/* crea la conexion con la base de datos */
		driver = get_driver_instance();
		//con = driver->connect("tcp://127.0.0.1:3306", "root", "raspberry");
		con = driver->connect(direccionpuerto.c_str(), usuario_db.c_str(), clave_db.c_str());
		int timeout1 = timeout_mysql;
		con->setClientOption("MYSQL_OPT_CONNECT_TIMEOUT", &timeout1);
		/* nombre de la base de datos */
		con->setSchema(nombre_db.c_str());
		stmt = con->createStatement();

		//Verificacion y correccion de valores -NaN.
		/*if(valorstr.compare("-nan")==0){valorstr="NULL"; valorfloat=NULL;}
		
		SQL_STR = "INSERT INTO `%s`.`%s` (`id`, `timedate`, `fechatiempo`, `tag`, `valorstr`, `valorfloat`, `dispositivo`, `rastro`, `concentrador`, `nombredispositivo` ) VALUES (NULL, CURRENT_TIMESTAMP, '%s', '%s', '%s', '%f', '%i', '%s', '%s', '%s' );";
		
		sprintf(arrSQL, SQL_STR.c_str(), nombre_db.c_str(), dbtabla.c_str(), fechatiemporecibido.c_str(), tag.c_str(), valorstr.c_str(), valorfloat, dispositivo, rastro.c_str(), nombre_concentrador.c_str(), nombre_dispositivo.c_str());
	
		*/
		if(valorstr.compare("-nan")==0){
			
			SQL_STR = "INSERT INTO `%s`.`%s` (`id`, `timedate`, `fechatiempo`, `tag`, `valorstr`, `valorfloat`, `dispositivo`, `rastro`, `concentrador`, `nombredispositivo` ) VALUES (NULL, CURRENT_TIMESTAMP, '%s', '%s', NULL, NULL, '%i', '%s', '%s', '%s' );";
			sprintf(arrSQL, SQL_STR.c_str(), nombre_db.c_str(), dbtabla.c_str(), fechatiemporecibido.c_str(), tag.c_str(), dispositivo, rastro.c_str(), nombre_concentrador.c_str(), nombre_dispositivo.c_str());

		}
		
		else{
		
			SQL_STR = "INSERT INTO `%s`.`%s` (`id`, `timedate`, `fechatiempo`, `tag`, `valorstr`, `valorfloat`, `dispositivo`, `rastro`, `concentrador`, `nombredispositivo` ) VALUES (NULL, CURRENT_TIMESTAMP, '%s', '%s', '%s', '%f', '%i', '%s', '%s', '%s' );";
			sprintf(arrSQL, SQL_STR.c_str(), nombre_db.c_str(), dbtabla.c_str(), fechatiemporecibido.c_str(), tag.c_str(), valorstr.c_str(), valorfloat, dispositivo, rastro.c_str(), nombre_concentrador.c_str(), nombre_dispositivo.c_str());
		}
		////////////////////////////////////////////////////////////////////////////////////////
        //cout << "la instruccion sql es: " << arrSQL << endl;
		resultado = stmt->execute(arrSQL);

		//  delete res;
		delete stmt;
		delete con;
		return resultado;

	}
	catch (sql::SQLException& e) {
		GLOBAL_ACCESO_EXTERNO = false;
		imprimir_mensaje("falla en mysql: ", rojo);
		imprimir_mensaje("se deshabilito lectura mysql externo por " + cast_int_to_string(GLOBAL_periodo_reseteo_mysqlexterno) + " seg ", amarillo);
		std::string mensajeError = "ERROR: SQLException in " + std::string(__FILE__) +
                               "(" + std::string(__FUNCTION__) + ") on line " + std::to_string(__LINE__) +
                               ".# Error description: " + e.what() +
                               " (MySQL error code: " + std::to_string(e.getErrorCode()) +
                               ", SQLState: " + e.getSQLState() + " )";

    	std::cout << mensajeError << std::endl;
    	logger_loadconfig.registrarError(mensajeError);
		return -1;
	}


}


/*
Tabla: tbejecutar
idPrimaria	int(11)
ejecutar_tbcmd	tinyint(1)
tiempo_tbcmd	datetime
tiempo_segundos	bigint(20)
*/
/* LEE LA TABLA tbejecutar para saber si de debe ejecutar los comandos
de la tabla tbcmd
*/
int leer_tbejecutar(int& res_ejecutar, time_t& segundos, string& lote, string direccion_db, int puerto_db, string nombre_db, string usuario_db, string clave_db)
{

	if (direccion_db.compare(GLOBAL_dbext_direccion) == 0) {
		if (GLOBAL_habilitar_acceso_externo == 0) {
			imprimir_mensaje("en la configuracion configdb esta desactivado el acceso a internet", negro);
			return 0;
		}
		if (GLOBAL_ACCESO_EXTERNO == false) {
			imprimir_mensaje("se desactivo momentaneamente el acceso al servidor mysql externo", negro);
			return 0;
		}
		else {
			if (chequear_internet(GLOBAL_direccionip_dns)) {
				;
			}
			else {
				imprimir_mensaje("no hay internet, no se puede alcanzar el servidor externo", negro);
				return 0;
			}
		}

	}

	int id = 0;
	int ind = 0;
	string cadena;
	string direccionpuerto;
	direccionpuerto = "tcp://" + direccion_db + ":" + cast_int_to_string(puerto_db);
	int ejecutar_tbcmd = 0;
	string tiempo_tbcmd;
	string SQL_STR = "";
	char arrSQL[1024] = { 0 };
	memset(arrSQL, 0, 1024);
	string valor_ejecutar = "0";
	string valor_tiempo_str;
	string valor_segundos_str;
	time_t tiempoactual;
	bool   res_update = false;
	try {
		sql::Driver* driver;
		sql::Connection* con;
		sql::Statement* stmt;
		sql::ResultSet* res;

		/* Crea una conexion */
		driver = get_driver_instance();
		con = driver->connect(direccionpuerto.c_str(), usuario_db.c_str(), clave_db.c_str()); //GLOBAL_usuario_db.c_str(), GLOBAL_clave_db.c_str());
		int timeout1 = timeout_mysql;
		con->setClientOption("MYSQL_OPT_CONNECT_TIMEOUT", &timeout1);
		/* Conecta con la base de datos */
		con->setSchema(nombre_db.c_str());

		stmt = con->createStatement();
		sprintf(arrSQL, "SELECT * FROM `tbejecutar` WHERE `concentrador` = '%s' ORDER BY `id` DESC LIMIT 1", GLOBAL_nombre_concentrador.c_str());
		res = stmt->executeQuery(arrSQL);

		while (res->next() && (ind < 64)) {
			id = res->getInt("id"); //
			ejecutar_tbcmd = res->getInt("ejecutar_tbcmd"); //
			tiempo_tbcmd = res->getString("tiempo_tbcmd"); //
			lote = res->getString("lote"); //
			segundos = cast_string_to_double((string)res->getString("tiempo_segundos")); //
			ind++;
		}
		memset(arrSQL, 0, 1024);
		if (ejecutar_tbcmd) {
			tiempoactual = time(NULL);
			valor_tiempo_str = cast_time_to_datetime(tiempoactual);
			valor_segundos_str = cast_double_to_string(tiempoactual);
			SQL_STR = "UPDATE `%s`.`tbejecutar` SET `ejecutar_tbcmd` = '%s', `tiempo_tbcmd` = '%s', `tiempo_segundos` = '%s' WHERE `tbejecutar`.`id` = %d;";
			sprintf(arrSQL, SQL_STR.c_str(), nombre_db.c_str(), valor_ejecutar.c_str(), valor_tiempo_str.c_str(), valor_segundos_str.c_str(), id);
			res_update = stmt->execute(arrSQL);
		};
		delete res;
		delete stmt;
		delete con;
		res_ejecutar = ejecutar_tbcmd;
		return (int)res_update;

	}

	catch (sql::SQLException& e) {
		GLOBAL_ACCESO_EXTERNO = false;
		imprimir_mensaje("falla en mysql: ", rojo);
		imprimir_mensaje("se deshabilito lectura mysql externo por " + cast_int_to_string(GLOBAL_periodo_reseteo_mysqlexterno) + " seg ", amarillo);
		std::string mensajeError = "ERROR: SQLException in " + std::string(__FILE__) +
                               "(" + std::string(__FUNCTION__) + ") on line " + std::to_string(__LINE__) +
                               ".# Error description: " + e.what() +
                               " (MySQL error code: " + std::to_string(e.getErrorCode()) +
                               ", SQLState: " + e.getSQLState() + " )";

    	std::cout << mensajeError << std::endl;
    	logger_loadconfig.registrarError(mensajeError);
		return -1;
	}


}

// si retorna 0, es porque no hay comandos  a ejecutar
// LA BASE DE DATOS TABLA dbcmd LA LLEVA A MEMORIA RAM
int leer_tbcmd(string direccion_db, int puerto_db, string nombre_db, string usuario_db, string clave_db)
{

	if (direccion_db.compare(GLOBAL_dbext_direccion) == 0) {

		if (GLOBAL_habilitar_acceso_externo == 0) {
			imprimir_mensaje("en la configuracion configdb esta desactivado el acceso a internet", negro);
			return 0;
		}

		if (GLOBAL_ACCESO_EXTERNO == false) {
			imprimir_mensaje("se desactivo momentaneamente el acceso al servidor mysql externo", negro);
			return 0;
		}
		else {
			if (chequear_internet(GLOBAL_direccionip_dns)) {
				;
			}
			else {
				imprimir_mensaje("no hay internet, no se puede alcanzar el servidor externo", negro);
				return 0;
			}
		}

	}

	int id = 0;
	int ind = 0;  // contador de comandos a ejecutar
	string cadena;
	string direccionpuerto;
	direccionpuerto = "tcp://" + direccion_db + ":" + cast_int_to_string(puerto_db);

	string cadenaquerycmd = "select `tbcmd`.`id` AS `id`,";
	cadenaquerycmd += "`tbdispositivos`.`nombre` AS `nombre`,";
	cadenaquerycmd += "`tbdispositivos`.`direccion_ip` AS `direccion_ip`,";
	cadenaquerycmd += "`tbdispositivos`.`puerto` AS `puerto`,";
	cadenaquerycmd += "`tbdispositivos`.`direccion_esclavo` AS `direccion_esclavo`,";
	cadenaquerycmd += "`tbdispositivos`.`interface` AS `interface`,";
	cadenaquerycmd += "`tbdispositivos`.`velocidad` AS `velocidad`,";
	cadenaquerycmd += "`tbdispositivos`.`paridad` AS `paridad`,";
	cadenaquerycmd += "`tbdispositivos`.`bitsparada` AS `bitsparada`,";
	cadenaquerycmd += "`tbdispositivos`.`modelo` AS `equipo`,";
	cadenaquerycmd += "`tbdispositivos`.`usuario` AS `usuario`,";
	cadenaquerycmd += "`tbdispositivos`.`password` AS `password`,";
	cadenaquerycmd += "`tbdispositivos`.`protocolo` AS `protocolo`,";
	cadenaquerycmd += "`tbdispositivos`.`habilitar` AS `habilitar`,";
	cadenaquerycmd += "`tbcmd`.`registro` AS `registro`,";
	cadenaquerycmd += "`tbcmd`.`valor` AS `valor`,";
	cadenaquerycmd += "`tbcmd`.`cmd_datetime` AS `datetime`,";
	cadenaquerycmd += "`tbcmd`.`hab_ejecutar` AS `hab_ejecutar`,";
	cadenaquerycmd += "`tbcmd`.`concentrador` AS `concentrador`, ";
	cadenaquerycmd += "`tbcmd`.`lote` AS `lote` ";
	cadenaquerycmd += "from (`tbdispositivos` join `tbcmd` ";
	cadenaquerycmd += "on((`tbdispositivos`.`nombre` = `tbcmd`.`nombredispositivo`) and"; cadenaquerycmd += "(`tbdispositivos`.`concentrador` = `tbcmd`.`concentrador`) )) ";
	cadenaquerycmd += "order by `tbdispositivos`.`id`";

	char cadenaquerycmd3[2000];
	memset(cadenaquerycmd3, 0, 2000);

	sprintf(cadenaquerycmd3, "SELECT COUNT(`id`) as contador FROM (%s) as vistacmd where `concentrador` = '%s'", cadenaquerycmd.c_str(), GLOBAL_nombre_concentrador.c_str());
	try {
		sql::Driver* driver;
		sql::Connection* con;
		sql::Statement* stmt;
		sql::ResultSet* res;

		/* Crea una conexion */
		driver = get_driver_instance();
		con = driver->connect(direccionpuerto.c_str(), usuario_db.c_str(), clave_db.c_str());
		int timeout1 = timeout_mysql;
		con->setClientOption("MYSQL_OPT_CONNECT_TIMEOUT", &timeout1);
		/* Conecta con la base de datos */
		con->setSchema(nombre_db.c_str());

		stmt = con->createStatement();
		res = stmt->executeQuery(cadenaquerycmd3);
		while (res->next()) {
			GLOBAL_contador_cmd = res->getInt("contador");
		}

		cmd = new cls_cmd[GLOBAL_contador_cmd + 1];
		memset(cadenaquerycmd3, 0, 2000);
		sprintf(cadenaquerycmd3, "SELECT * FROM (%s) as vistacmd where `concentrador` = '%s' ORDER BY `id` ASC", cadenaquerycmd.c_str(), GLOBAL_nombre_concentrador.c_str());
		res = stmt->executeQuery(cadenaquerycmd3);
		//cout << cadenaquerycmd3 << endl;
		while (res->next() && (ind < 128)) {
			cmd[ind].id = res->getInt("id");
			cmd[ind].nombre = res->getString("nombre"); //
			cmd[ind].direccion_ip = res->getString("direccion_ip"); //
			cmd[ind].puerto = res->getInt("puerto"); //
			cmd[ind].direccion_esclavo = res->getInt("direccion_esclavo"); //
			cmd[ind].interface1 = res->getString("interface"); //
			cmd[ind].velocidad = res->getInt("velocidad"); //
			cmd[ind].paridad = res->getString("paridad"); //
			cmd[ind].bitsparada = res->getInt("bitsparada"); //
			cmd[ind].equipo = res->getString("equipo"); //

			cmd[ind].usuario = res->getString("usuario"); //
			cmd[ind].password = res->getString("password"); //
			cmd[ind].habilitar = ((res->getInt("habilitar")) != 0); //
			cmd[ind].protocolo = res->getString("protocolo"); //
			cmd[ind].registro = res->getInt("registro"); //
			cmd[ind].clsvalor = res->getInt("valor"); //
			cmd[ind].datetime = res->getString("datetime"); //
			cmd[ind].hab_ejecutar = ((res->getInt("hab_ejecutar")) != 0); //
			cmd[ind].lote = res->getString("lote"); //
			ind++;

			//cout << "\t valores: " << cadena << "\t" << intTime1 << endl; 	

			//   cout << res->getString(1) << endl;
		}
		delete res;
		delete stmt;
		delete con;
		return (ind);

	}
	catch (sql::SQLException& e) {
		GLOBAL_ACCESO_EXTERNO = false;
		imprimir_mensaje("falla en mysql: ", rojo);
		imprimir_mensaje("se deshabilito lectura mysql externo por " + cast_int_to_string(GLOBAL_periodo_reseteo_mysqlexterno) + " seg ", amarillo);
		std::string mensajeError = "ERROR: SQLException in " + std::string(__FILE__) +
                               "(" + std::string(__FUNCTION__) + ") on line " + std::to_string(__LINE__) +
                               ".# Error description: " + e.what() +
                               " (MySQL error code: " + std::to_string(e.getErrorCode()) +
                               ", SQLState: " + e.getSQLState() + " )";

    	std::cout << mensajeError << std::endl;
    	logger_loadconfig.registrarError(mensajeError);
		return -1;
	}


}



/* escribe en la tabla tbcmd el momento despues que se ejecuta el comando
de la tabla tbcmd
*/
int escribir_tbcmd_fechahora(int id, string direccion_db, int puerto_db, string nombre_db, string usuario_db, string clave_db)
{

	if (direccion_db.compare(GLOBAL_dbext_direccion) == 0) {
		if (GLOBAL_habilitar_acceso_externo == 0) {
			imprimir_mensaje("en la configuracion configdb esta desactivado el acceso a internet", negro);
			return 0;
		}

		if (GLOBAL_ACCESO_EXTERNO == false) {
			imprimir_mensaje("se desactivo momentaneamente el acceso al servidor mysql externo", negro);
			return 0;
		}
		else {
			if (chequear_internet(GLOBAL_direccionip_dns)) {
				;
			}
			else {
				imprimir_mensaje("no hay internet, no se puede alcanzar el servidor externo", negro);
				return 0;
			}
		}

	}

	string cadena;
	string direccionpuerto;
	direccionpuerto = "tcp://" + direccion_db + ":" + cast_int_to_string(puerto_db);

	string SQL_STR = "";
	char arrSQL[1024] = { 0 };
	memset(arrSQL, 0, 1024);
	string valor_tiempo_str;
	time_t tiempoactual;
	bool   res_update = false;
	try {
		sql::Driver* driver;
		sql::Connection* con;
		sql::Statement* stmt;


		/* Crea una conexion */
		driver = get_driver_instance();
		con = driver->connect(direccionpuerto.c_str(), usuario_db.c_str(), clave_db.c_str()); //GLOBAL_usuario_db.c_str(), GLOBAL_clave_db.c_str());
		int timeout1 = timeout_mysql;
		con->setClientOption("MYSQL_OPT_CONNECT_TIMEOUT", &timeout1);
		/* Conecta con la base de datos */
		con->setSchema(nombre_db.c_str());

		stmt = con->createStatement();

		tiempoactual = time(NULL);
		valor_tiempo_str = cast_time_to_datetime(tiempoactual);

		SQL_STR = "UPDATE `%s`.`tbcmd` SET `cmd_datetime` = '%s' WHERE `tbcmd`.`id` = %i;";
		sprintf(arrSQL, SQL_STR.c_str(), nombre_db.c_str(), valor_tiempo_str.c_str(), id);
		res_update = stmt->execute(arrSQL);

		delete stmt;
		delete con;
		return (int)res_update;

	}

	catch (sql::SQLException& e) {
		GLOBAL_ACCESO_EXTERNO = false;
		imprimir_mensaje("falla en mysql: ", rojo);
		imprimir_mensaje("se deshabilito lectura mysql externo por " + cast_int_to_string(GLOBAL_periodo_reseteo_mysqlexterno) + " seg ", amarillo);
		std::string mensajeError = "ERROR: SQLException in " + std::string(__FILE__) +
                               "(" + std::string(__FUNCTION__) + ") on line " + std::to_string(__LINE__) +
                               ".# Error description: " + e.what() +
                               " (MySQL error code: " + std::to_string(e.getErrorCode()) +
                               ", SQLState: " + e.getSQLState() + " )";

    	std::cout << mensajeError << std::endl;
    	logger_loadconfig.registrarError(mensajeError);
		return -1;
	}


}



/* mantiene el largo de las tablas valores y instantaneos
limpia los registros mas viejos*/



int borrar_registros(double dias, string direccion_db, int puerto_db, string nombre_db, string usuario_db, string clave_db, string dbtabla)
{
    // Validaciones de red e interfaz
    if (direccion_db.compare(GLOBAL_dbext_direccion) == 0) {
        if (GLOBAL_habilitar_acceso_externo == 0) {
            imprimir_mensaje("en la configuracion configdb esta desactivado el acceso a internet", negro);
            return 0;
        }
        if (GLOBAL_ACCESO_EXTERNO == false) {
            imprimir_mensaje("se desactivo momentaneamente el acceso al servidor mysql externo", negro);
            return 0;
        }
        else {
            if (!chequear_internet(GLOBAL_direccionip_dns)) {
                imprimir_mensaje("no hay internet, no se puede alcanzar el servidor externo", negro);
                return 0;
            }
        }
    }

    string direccionpuerto = "tcp://" + direccion_db + ":" + cast_int_to_string(puerto_db);

    // 1. Calcular la fecha limite directamente en C++ sin consultar a MySQL
    time_t tiempo_corte = time(NULL) - static_cast<time_t>(dias * 86400.0);
    string fechacorte_str = cast_time_to_datetime(tiempo_corte);

    imprimir_mensaje("borrar registros: bd: " + direccionpuerto, amarillo);
    imprimir_mensaje("borrar registros: conservando datos desde: " + fechacorte_str, amarillo);

    try {
        sql::Driver* driver = get_driver_instance();
        
        // Uso de smart pointers para garantizar liberacion automatica de memoria (RIIA)
        std::unique_ptr<sql::Connection> con(driver->connect(direccionpuerto.c_str(), usuario_db.c_str(), clave_db.c_str()));
        
        int timeout1 = timeout_mysql;
        con->setClientOption("MYSQL_OPT_CONNECT_TIMEOUT", &timeout1);
        con->setSchema(nombre_db.c_str());

        std::unique_ptr<sql::Statement> stmt(con->createStatement());

        // 2. Borrado por lotes (Chunks) de 10,000 registros para evitar bloqueos y timeouts
        const int LOTE_TAMANO = 10000;
        int total_eliminados = 0;
        int filas_afectadas = 0;

        string sql_delete = "DELETE FROM `" + dbtabla + "` WHERE `fechatiempo` < '" + fechacorte_str + "' LIMIT " + std::to_string(LOTE_TAMANO);

        do {
            // executeUpdate retorna el numero de filas eliminadas en este lote
            filas_afectadas = stmt->executeUpdate(sql_delete);
            total_eliminados += filas_afectadas;

            if (filas_afectadas > 0) {
                // Pausa tactica de 50ms para permitir que otros hilos/consultas accedan a la tabla
                std::this_thread::sleep_for(std::chrono::milliseconds(50));
            }

        } while (filas_afectadas >= LOTE_TAMANO);

        imprimir_mensaje("borrar registros: eliminacion completada. Total filas borradas: " + std::to_string(total_eliminados), verde);

        return total_eliminados;
    }
    catch (sql::SQLException& e) {
        GLOBAL_ACCESO_EXTERNO = false;
        imprimir_mensaje("falla en mysql: ", rojo);
        imprimir_mensaje("se deshabilito lectura mysql externo por " + cast_int_to_string(GLOBAL_periodo_reseteo_mysqlexterno) + " seg ", amarillo);

        std::string mensajeError = "ERROR: SQLException in " + std::string(__FILE__) +
                                   "(" + std::string(__FUNCTION__) + ") on line " + std::to_string(__LINE__) +
                                   ".# Error description: " + e.what() +
                                   " (MySQL error code: " + std::to_string(e.getErrorCode()) +
                                   ", SQLState: " + e.getSQLState() + " )";

        std::cout << mensajeError << std::endl;
        logger_loadconfig.registrarError(mensajeError);
        return -1;
    }
}


// GLOBAL_nombre_concentrador
// SELECT `id`, `timedate`, `fechatiempo`, `tag`, `valorstr`, `valorfloat`, `dispositivo`, `rastro`, `concentrador` FROM `tbvalores`
// sincronizar tabla de dos bases de datos
int sincronizar_tablas(struct stdatabase* dblocal, struct stdatabase* dbexterna, string nombre, int limitederegistros) {

	if (dbexterna->direccion.compare(GLOBAL_dbext_direccion) == 0) {
		if (GLOBAL_habilitar_acceso_externo == 0) {
			imprimir_mensaje("en la configuracion configdb esta desactivado el acceso a internet", negro);
			return 0;
		}
		if (GLOBAL_ACCESO_EXTERNO == false) {
			imprimir_mensaje("se desactivo momentaneamente el acceso al servidor mysql externo", negro);
			return 0;
		}
		else {
			if (chequear_internet(GLOBAL_direccionip_dns)) {
				;
			}
			else {
				imprimir_mensaje("no hay internet, no se puede alcanzar el servidor externo", negro);
				return 0;
			}
		}

	}

	int id = 0;
	int ind = 0;  // contador de comandos a ejecutar
	int result_insertar;
	string campo_id, campo_timedate, campo_fechatiempo, campo_tag, campo_valorstr, campo_rastro, campo_nombre_concentrador, query1;
	string campo_nombre_dispositivo;
	double campo_valorfloat;
	int campo_dispositivo;
	string cadena;
	string direccionpuerto_ext;
	string direccionpuerto;
	string SQL_STR = "";
	string fechatiempostr;
	time_t fechaultima_externa; // ultimo valor introducido en tabla tbvalores
	char arrSQL_ext[1024] = { 0 };
	memset(arrSQL_ext, 0, 1024);

	char arrSQL[1024] = { 0 };
	memset(arrSQL, 0, 1024);

	fechatiempostr = "2019-1-1 00:00:00";
	direccionpuerto_ext = "tcp://" + (dbexterna->direccion) + ":" + cast_int_to_string(dbexterna->puerto);

	query1 = "SELECT `fechatiempo`, `concentrador` FROM `tbvalores` WHERE `concentrador` = '%s' ORDER BY `fechatiempo` DESC LIMIT 1 ";
	sprintf(arrSQL_ext, query1.c_str(), GLOBAL_nombre_concentrador.c_str());


	try {
		sql::Driver* driver;
		sql::Connection* con;
		sql::Statement* stmt;
		sql::ResultSet* res;

		sql::Driver* driver_ext;
		sql::Connection* con_ext;
		sql::Statement* stmt_ext;
		sql::ResultSet* res_ext;

		/* Crea una conexion con la base de datos externa */
		driver_ext = get_driver_instance();
		con_ext = driver_ext->connect(direccionpuerto_ext.c_str(), (dbexterna->usuario).c_str(), (dbexterna->clave).c_str());
		int timeout1 = timeout_mysql;
		con_ext->setClientOption("MYSQL_OPT_CONNECT_TIMEOUT", &timeout1);
		/* Conecta con la base de datos externa */
		con_ext->setSchema((dbexterna->nombre).c_str());

		stmt_ext = con_ext->createStatement();
		res_ext = stmt_ext->executeQuery(arrSQL_ext);
		ind = 0;
		while (res_ext->next() && (ind < 1)) {
			fechatiempostr = res_ext->getString("fechatiempo"); //
			ind++;
		}
		// aqui se obstiene la fecha y tiempo del ultimo valor que este concentrador introdujo
		// en la base de datos externa (nube)
		fechaultima_externa = cast_datetime_to_time(fechatiempostr);

		/* Crea una conexion con la base de datos local */
		direccionpuerto = "tcp://" + (dblocal->direccion) + ":" + cast_int_to_string(dblocal->puerto);
		driver = get_driver_instance();
		con = driver->connect(direccionpuerto.c_str(), (dblocal->usuario).c_str(), (dblocal->clave).c_str());
		con->setClientOption("MYSQL_OPT_CONNECT_TIMEOUT", &timeout1);
		/* Conecta con la base de datos local */
		con->setSchema((dblocal->nombre).c_str());

		query1 = "SELECT * FROM `%s` WHERE `fechatiempo` > '%s' ORDER BY `fechatiempo` ASC LIMIT %i ";
		sprintf(arrSQL, query1.c_str(), nombre.c_str(), fechatiempostr.c_str(), limitederegistros);
		stmt = con->createStatement();
		res = stmt->executeQuery(arrSQL);
		ind = 0;
		while (res->next() && (ind < limitederegistros)) {
			campo_id = res->getString("id");
			campo_timedate = res->getString("timedate");
			campo_fechatiempo = res->getString("fechatiempo");
			campo_tag = res->getString("tag");
			campo_valorstr = res->getString("valorstr");
			campo_valorfloat = cast_string_to_double(res->getString("valorfloat"));
			campo_dispositivo = res->getInt("dispositivo");
			campo_rastro = res->getString("rastro");
			campo_nombre_concentrador = res->getString("concentrador");
			campo_nombre_dispositivo = res->getString("nombredispositivo");

			result_insertar = insertardb(cast_datetime_to_time(campo_fechatiempo), campo_tag, campo_valorstr, campo_valorfloat, campo_dispositivo, campo_rastro, campo_nombre_concentrador, campo_nombre_dispositivo, dbexterna->direccion, dbexterna->puerto, dbexterna->nombre, dbexterna->usuario, dbexterna->clave, nombre);
			ind++;
		}

		delete res;
		delete stmt;
		delete con;
		delete res_ext;
		delete stmt_ext;
		delete con_ext;


		return (ind);

	}
	catch (sql::SQLException& e) {
		GLOBAL_ACCESO_EXTERNO = false;
		imprimir_mensaje("falla en mysql: ", rojo);
		imprimir_mensaje("se deshabilito lectura mysql externo por " + cast_int_to_string(GLOBAL_periodo_reseteo_mysqlexterno) + " seg ", amarillo);
		std::string mensajeError = "ERROR: SQLException in " + std::string(__FILE__) +
                               "(" + std::string(__FUNCTION__) + ") on line " + std::to_string(__LINE__) +
                               ".# Error description: " + e.what() +
                               " (MySQL error code: " + std::to_string(e.getErrorCode()) +
                               ", SQLState: " + e.getSQLState() + " )";

    	std::cout << mensajeError << std::endl;
    	logger_loadconfig.registrarError(mensajeError);
		return -1;
	}

}




// leer tabla de definicion de alarmas
int actualizar_defalarmas(struct stdatabase* db) {

	if (db->direccion.compare(GLOBAL_dbext_direccion) == 0) {
		if (GLOBAL_habilitar_acceso_externo == 0) {
			imprimir_mensaje("en la configuracion configdb esta desactivado el acceso a internet", negro);
			return 0;
		}
		if (GLOBAL_ACCESO_EXTERNO == false) {
			imprimir_mensaje("se desactivo momentaneamente el acceso al servidor mysql externo", negro);
			return 0;
		}
		else {
			if (chequear_internet(GLOBAL_direccionip_dns)) {
				;
			}
			else {
				imprimir_mensaje("no hay internet, no se puede alcanzar el servidor externo", negro);
				return 0;
			}
		}

	}

	int id = 0;
	int ind = 0;  // contador de comandos a ejecutar
	string activar = "";
	string desactivar = "";
	string direccionpuerto;
	string SQL_STR = "";
	string descripcion = "";
	int habilitar = 0;
	string query1 = "";
	string query2 = "";
	//string query1 = "SELECT COUNT(`id`) as contador FROM `tbdefalarmas` WHERE habilitar = 1";
	query1 += "select count(`id`) as `contador` from(";
	query1 += "SELECT `tbdefalarmas`.`id` as `id`,";
	query1 += "`tbdefalarmas`.`habilitar` as `habilitar`,";
	query1 += "`tbdispositivos`.`habilitar` AS `habilitardispo`";
	query1 += "FROM";
	query1 += "(`tbdefalarmas` join `tbdispositivos` on";
	query1 += "(`tbdefalarmas`.`dispositivo` = `tbdispositivos`.`nombre`)) order by `tbdefalarmas`.`id`";
	query1 += ") tabladefalarmas";
	query1 += " where `tabladefalarmas`.`habilitar`='1' and `tabladefalarmas`.`habilitardispo`='1'";

	//string query2 = "SELECT * FROM `tbdefalarmas` WHERE habilitar = 1";


	query2 += "SELECT `tbdefalarmas`.`id` as `id`,";
	query2 += "`tbdefalarmas`.`activar` as `activar`,";
	query2 += "`tbdefalarmas`.`desactivar` as `desactivar`,";
	query2 += "`tbdefalarmas`.`descripcion` as `descripcion`,";
	query2 += "`tbdefalarmas`.`accionactivar` as `accionactivar`,";
	query2 += "`tbdefalarmas`.`acciondesactivar` as `acciondesactivar`,";
	query2 += "`tbdefalarmas`.`dispositivo` as `dispositivo`,";
	query2 += "`tbdefalarmas`.`tag` as `tag`,";
	query2 += "`tbdefalarmas`.`habilitar` as `habilitar`,";
	query2 += "`tbdefalarmas`.`valorAlarma` as `valorAlarma`,";
	query2 += "`tbdispositivos`.`habilitar` AS `habilitardispo`";
	query2 += "FROM";
	query2 += "(`tbdefalarmas` join `tbdispositivos` on";
	query2 += "(`tbdefalarmas`.`dispositivo` = `tbdispositivos`.`nombre`))";
	query2 += " where `tbdefalarmas`.`habilitar` = '1' and `tbdispositivos`.`habilitar` = '1'";
	query2 += "order by `tbdefalarmas`.`id`";

	char arrSQL[1024] = { 0 };
	memset(arrSQL, 0, 1024);
	direccionpuerto = "tcp://" + (db->direccion) + ":" + cast_int_to_string(db->puerto);
	sprintf(arrSQL, query1.c_str());

	cout << "direccion:" << direccionpuerto << endl;
	cout << "base de datos:" << (db->nombre) << endl;

	try {
		sql::Driver* driver;
		sql::Connection* con;
		sql::Statement* stmt;
		sql::ResultSet* res;

		/* Crea una conexion con la base de datos externa */
		driver = get_driver_instance();
		con = driver->connect(direccionpuerto.c_str(), (db->usuario).c_str(), (db->clave).c_str());
		int timeout1 = timeout_mysql;
		con->setClientOption("MYSQL_OPT_CONNECT_TIMEOUT", &timeout1);
		/* Conecta con la base de datos */
		con->setSchema((db->nombre).c_str());
		stmt = con->createStatement();
		res = stmt->executeQuery(arrSQL);

		while (res->next()) {
			GLOBAL_contador_defalarmas = res->getInt("contador");
		}
		imprimir_mensaje("cantidad de alarmas definidas: " + cast_int_to_string(GLOBAL_contador_defalarmas));
		memset(arrSQL, 0, 1024);
		sprintf(arrSQL, query2.c_str());
		res = stmt->executeQuery(arrSQL);
		ind = 0;


		// se reserva memoria
		defalarmas = new cls_defalarmas[GLOBAL_contador_defalarmas + 1];

		while (res->next() && (ind < GLOBAL_contador_defalarmas)) {
			defalarmas[ind].id = res->getInt("id"); //
			defalarmas[ind].expresionactivar = res->getString("activar");
			defalarmas[ind].expresiondesactivar = res->getString("desactivar");
			defalarmas[ind].descripcion = res->getString("descripcion");
			defalarmas[ind].habilitar = res->getInt("habilitar") != 0; //
			defalarmas[ind].accionactivar = res->getString("accionactivar"); //
			defalarmas[ind].acciondesactivar = res->getString("acciondesactivar"); //
			defalarmas[ind].clsequipo_mbtcp = &equipo_mbtcp[0];
			defalarmas[ind].clscantidad_equipo_mbtcp = GLOBAL_contador_equipos_mbtcp;
			defalarmas[ind].clsequipo_mbserial = &equipo_mbserial[0];
			defalarmas[ind].clscantidad_equipo_mbserial = GLOBAL_contador_equipos_mbserial;
			defalarmas[ind].clsequipo_rs232 = &equipo_rs232[0];
			defalarmas[ind].clscantidad_equipo_rs232 = GLOBAL_contador_equipos_rs232;

			defalarmas[ind].clsemaillist = &emaillist[0];
			defalarmas[ind].clscantidademaillist = GLOBAL_contador_directorio;
			defalarmas[ind].clsdispositivo = res->getString("dispositivo");
			defalarmas[ind].clstag = res->getString("tag");
			defalarmas[ind].clscmd = &cmd[0];
			defalarmas[ind].clscantidad_cmd = GLOBAL_contador_cmd;
			defalarmas[ind].clsvalorAlarma = res->getString("valorAlarma");

			ind++;
		}

		GLOBAL_contador_defalarmas = ind;

		delete res;
		delete stmt;
		delete con;


		return (ind);

	}
	catch (sql::SQLException& e) {
		GLOBAL_ACCESO_EXTERNO = false;
		imprimir_mensaje("falla en mysql: ", rojo);
		imprimir_mensaje("se deshabilito lectura mysql externo por " + cast_int_to_string(GLOBAL_periodo_reseteo_mysqlexterno) + " seg ", amarillo);
		std::string mensajeError = "ERROR: SQLException in " + std::string(__FILE__) +
                               "(" + std::string(__FUNCTION__) + ") on line " + std::to_string(__LINE__) +
                               ".# Error description: " + e.what() +
                               " (MySQL error code: " + std::to_string(e.getErrorCode()) +
                               ", SQLState: " + e.getSQLState() + " )";

    	std::cout << mensajeError << std::endl;
    	logger_loadconfig.registrarError(mensajeError);
		return -1;
	}

}



// leer tabla de directorio
int actualizar_directorio(struct stdatabase* db) {

	if (db->direccion.compare(GLOBAL_dbext_direccion) == 0) {
		if (GLOBAL_habilitar_acceso_externo == 0) {
			imprimir_mensaje("en la configuracion configdb esta desactivado el acceso a internet", negro);
			return 0;
		}
		if (GLOBAL_ACCESO_EXTERNO == false) {
			imprimir_mensaje("se desactivo momentaneamente el acceso al servidor mysql externo", negro);
			return 0;
		}
		else {
			if (chequear_internet(GLOBAL_direccionip_dns)) {
				;
			}
			else {
				imprimir_mensaje("no hay internet, no se puede alcanzar el servidor externo", negro);
				return 0;
			}
		}

	}


	int id = 0;
	int ind = 0;  // contador de comandos a ejecutar
	string direccionpuerto;
	string SQL_STR = "";
	string descripcion = "";
	int habilitar = 0;

	string query1 = "SELECT COUNT(`id`) as contador FROM `tbdirectorio`";
	string query2 = "SELECT * FROM `tbdirectorio`";
	char arrSQL[1024] = { 0 };
	memset(arrSQL, 0, 1024);
	direccionpuerto = "tcp://" + (db->direccion) + ":" + cast_int_to_string(db->puerto);
	sprintf(arrSQL, query1.c_str());

	//cout << "direccion:" << direccionpuerto << endl;
	//cout << "base de datos:" << (db -> nombre) << endl;

	try {
		sql::Driver* driver;
		sql::Connection* con;
		sql::Statement* stmt;
		sql::ResultSet* res;

		/* Crea una conexion con la base de datos externa */
		driver = get_driver_instance();
		con = driver->connect(direccionpuerto.c_str(), (db->usuario).c_str(), (db->clave).c_str());
		int timeout1 = timeout_mysql;
		con->setClientOption("MYSQL_OPT_CONNECT_TIMEOUT", &timeout1);
		/* Conecta con la base de datos */
		con->setSchema((db->nombre).c_str());
		stmt = con->createStatement();
		res = stmt->executeQuery(arrSQL);

		while (res->next()) {
			GLOBAL_contador_directorio = res->getInt("contador");
		}
		memset(arrSQL, 0, 1024);
		sprintf(arrSQL, query2.c_str());
		res = stmt->executeQuery(arrSQL);
		ind = 0;


		// se reserva memoria
		emaillist = new cls_email[GLOBAL_contador_directorio + 1];

		while (res->next() && (ind < GLOBAL_contador_directorio)) {

			if (&emaillist[ind]) {
				emaillist[ind].id = res->getInt("id"); //
				emaillist[ind].from = res->getString("emailfrom");
				emaillist[ind].to = res->getString("emailto");
				emaillist[ind].cc = res->getString("emailcc");
				emaillist[ind].asunto = res->getString("asunto"); //
				emaillist[ind].mensaje = res->getString("mensaje"); //

			}

			ind++;
		}

		GLOBAL_contador_directorio = ind;

		delete res;
		delete stmt;
		delete con;


		return (ind);

	}
	catch (sql::SQLException& e) {
		GLOBAL_ACCESO_EXTERNO = false;
		imprimir_mensaje("falla en mysql: ", rojo);
		imprimir_mensaje("se deshabilito lectura mysql externo por " + cast_int_to_string(GLOBAL_periodo_reseteo_mysqlexterno) + " seg ", amarillo);
		std::string mensajeError = "ERROR: SQLException in " + std::string(__FILE__) +
                               "(" + std::string(__FUNCTION__) + ") on line " + std::to_string(__LINE__) +
                               ".# Error description: " + e.what() +
                               " (MySQL error code: " + std::to_string(e.getErrorCode()) +
                               ", SQLState: " + e.getSQLState() + " )";

    	std::cout << mensajeError << std::endl;
    	logger_loadconfig.registrarError(mensajeError);
		return -1;
	}

}



// revuelve 0 si no consigue
//devuelve 1 si consigue el item buscado
int buscarentabla(struct stdatabase* db, string tabla, string campo, string valor)
{

	int ind = 0;  // contador de comandos a ejecutar
	string cadena;
	string direccionpuerto;
	direccionpuerto = "tcp://" + db->direccion + ":" + cast_int_to_string(db->puerto);
	string cadenasql = "SELECT * FROM " + tabla + " WHERE " + campo + " = '" + valor + "'";
	try {
		sql::Driver* driver;
		sql::Connection* con;
		sql::Statement* stmt;
		sql::ResultSet* res;

		/* Crea una conexion */
		driver = get_driver_instance();
		con = driver->connect(direccionpuerto.c_str(), (db->usuario).c_str(), (db->clave).c_str());
		int timeout1 = 3;
		con->setClientOption("MYSQL_OPT_CONNECT_TIMEOUT", &timeout1);
		con->setClientOption("MYSQL_OPT_READ_TIMEOUT", &timeout1);


		/* Conecta con la base de datos */
		con->setSchema((db->nombre).c_str());

		stmt = con->createStatement();

		res = stmt->executeQuery(cadenasql.c_str());

		while (res->next() && (ind < 1)) {

			ind++;

		}
		delete res;
		delete stmt;
		delete con;
		return (ind);

	}
	catch (sql::SQLException& e) {
		std::string mensajeError = "ERROR: SQLException in " + std::string(__FILE__) +
                               "(" + std::string(__FUNCTION__) + ") on line " + std::to_string(__LINE__) +
                               ".# Error description: " + e.what() +
                               " (MySQL error code: " + std::to_string(e.getErrorCode()) +
                               ", SQLState: " + e.getSQLState() + " )";

    	std::cout << mensajeError << std::endl;
    	logger_loadconfig.registrarError(mensajeError);
		return -1;
	}


}


// revuelve 0 si no consigue
//devuelve 1 si consigue el item buscado
int buscarentabla3(struct stdatabase* db, string tabla, string campo1, string campo2, string campo3, string valor1, string valor2, string valor3)
{

	int ind = 0;  // contador de comandos a ejecutar
	string cadena;
	string direccionpuerto;
	direccionpuerto = "tcp://" + db->direccion + ":" + cast_int_to_string(db->puerto);
	string cadenasql = "SELECT * FROM " + tabla + " WHERE `" + campo1 + "` = '" + valor1 + "' AND ";
	cadenasql = cadenasql + "`" + campo2 + "` = '" + valor2 + "' AND ";
	cadenasql = cadenasql + "`" + campo3 + "` = '" + valor3 + "'";
	try {
		sql::Driver* driver;
		sql::Connection* con;
		sql::Statement* stmt;
		sql::ResultSet* res;

		/* Crea una conexion */
		driver = get_driver_instance();
		con = driver->connect(direccionpuerto.c_str(), (db->usuario).c_str(), (db->clave).c_str());
		int timeout1 = 3;
		con->setClientOption("MYSQL_OPT_CONNECT_TIMEOUT", &timeout1);
		con->setClientOption("MYSQL_OPT_READ_TIMEOUT", &timeout1);


		/* Conecta con la base de datos */
		con->setSchema((db->nombre).c_str());

		stmt = con->createStatement();

		res = stmt->executeQuery(cadenasql.c_str());

		while (res->next() && (ind < 1)) {

			ind++;

		}
		delete res;
		delete stmt;
		delete con;
		return (ind);

	}
	catch (sql::SQLException& e) {
		std::string mensajeError = "ERROR: SQLException in " + std::string(__FILE__) +
                               "(" + std::string(__FUNCTION__) + ") on line " + std::to_string(__LINE__) +
                               ".# Error description: " + e.what() +
                               " (MySQL error code: " + std::to_string(e.getErrorCode()) +
                               ", SQLState: " + e.getSQLState() + " )";

    	std::cout << mensajeError << std::endl;
    	logger_loadconfig.registrarError(mensajeError);
		return -1;
	}


}


// actualiza los valores de cualquier tabla
// el resultado es 0 si todo ok, y -1 si hay algun error


int actualizar_valor(struct stdatabase* db, string tabla, int indice, string* campo, string* valor, int cantidad) {


	if (db->direccion.compare(GLOBAL_dbext_direccion) == 0) {
		if (GLOBAL_habilitar_acceso_externo == 0) {
			imprimir_mensaje("en la configuracion configdb esta desactivado el acceso a internet", negro);
			return 0;
		}
		if (GLOBAL_ACCESO_EXTERNO == false) {
			imprimir_mensaje("se desactivo momentaneamente el acceso al servidor mysql externo", negro);
			return 0;
		}
		else {
			if (chequear_internet(GLOBAL_direccionip_dns)) {
				;
			}
			else {
				imprimir_mensaje("no hay internet, no se puede alcanzar el servidor externo", negro);
				return 0;
			}
		}

	}

	bool resultado = false;
	int res_int = 0;
	string direccionpuerto;
	string SQL_STR = "";
	string query1 = "UPDATE `" + tabla + "` SET ";
	for (int i = 0; i < cantidad; i++) {
		
		//Correccion 25/10/24
		//Verificacion y correccion de valores -NaN. Observacion: Este codigo puede mejorarse haciendo una funcion o clase aparte ya que se utiliza en varias partes del codigo. 
		if(valor[i].compare("-nan")==0){valor[i]="NULL";}

		///////////////////////////////////////////////////////////////////////
		//Modificacion para valores NULL - 28/10/24

		if(valor[i].compare("NULL")==0){

			if (i == (cantidad - 1)) {
				query1 = query1 + "`" + campo[i] + "` = " + valor[i];
			}
			else {
				query1 = query1 + "`" + campo[i] + "` = " + valor[i] + ",";
			}
		}

		else{
			if (i == (cantidad - 1)) {
				query1 = query1 + "`" + campo[i] + "` = '" + valor[i] + "'";
			}
			else {
				query1 = query1 + "`" + campo[i] + "` = '" + valor[i] + "',";
			}
		}
        
        /*
		if (i == (cantidad - 1)) {
			query1 = query1 + "`" + campo[i] + "` = '" + valor[i] + "'";
		}
		else {
			query1 = query1 + "`" + campo[i] + "` = '" + valor[i] + "',";
		}
        */
	}

	query1 = query1 + " WHERE `" + campo[indice] + "` = '" + valor[indice] + "'";

	char arrSQL[1024] = { 0 };

	//arrSQL = new char[query1.size()+1];
	//cout << "query1:" << query1 << endl;

	memset(arrSQL, 0, 1024);
	direccionpuerto = "tcp://" + (db->direccion) + ":" + cast_int_to_string(db->puerto);
	sprintf(arrSQL, query1.c_str());
	imprimir_mensaje("actualizacion valores: " + query1, negro);
	//printf(arrSQL); printf("\n");

	try {
		sql::Driver* driver;
		sql::Connection* con;
		sql::Statement* stmt;


		/* Crea una conexion con la base de datos externa */
		driver = get_driver_instance();
		con = driver->connect(direccionpuerto.c_str(), (db->usuario).c_str(), (db->clave).c_str());
		int timeout1 = timeout_mysql;
		con->setClientOption("MYSQL_OPT_CONNECT_TIMEOUT", &timeout1);
		/* Conecta con la base de datos */
		con->setSchema((db->nombre).c_str());
		stmt = con->createStatement();
		resultado = stmt->execute(arrSQL);

		//cout << "resultado:" << resultado << endl;


		delete stmt;
		delete con;
		//delete arrSQL;

		res_int = resultado ? -1 : 0;

		//cout << "resultado entero:" << res_int << endl;

		return (res_int);

	}
	catch (sql::SQLException& e) {

		std::string mensajeError = "ERROR: SQLException in " + std::string(__FILE__) +
                               "(" + std::string(__FUNCTION__) + ") on line " + std::to_string(__LINE__) +
                               ".# Error description: " + e.what() +
                               " (MySQL error code: " + std::to_string(e.getErrorCode()) +
                               ", SQLState: " + e.getSQLState() + " )";

    	std::cout << mensajeError << std::endl;
    	logger_loadconfig.registrarError(mensajeError);
		return -1;
	}

}


// actualiza los valores de cualquier tabla
// el resultado es 0 si todo ok, y -1 si hay algun error


int actualizar_valor3(struct stdatabase* db, string tabla, int indice1, int indice2, int indice3, string* campo, string* valor, int cantidad) {


	if (db->direccion.compare(GLOBAL_dbext_direccion) == 0) {
		if (GLOBAL_habilitar_acceso_externo == 0) {
			imprimir_mensaje("en la configuracion configdb esta desactivado el acceso a internet", negro);
			return 0;
		}
		if (GLOBAL_ACCESO_EXTERNO == false) {
			imprimir_mensaje("se desactivo momentaneamente el acceso al servidor mysql externo", negro);
			return 0;
		}
		else {
			if (chequear_internet(GLOBAL_direccionip_dns)) {
				;
			}
			else {
				imprimir_mensaje("no hay internet, no se puede alcanzar el servidor externo", negro);
				return 0;
			}
		}

	}

	bool resultado = false;
	int res_int = 0;
	string direccionpuerto;
	string SQL_STR = "";
	string query1 = "UPDATE `" + tabla + "` SET ";
	for (int i = 0; i < cantidad; i++) {
		
		//Correccion 25/10/24
		//Verificacion y correccion de valores -NaN.

        if(valor[i].compare("-nan")==0){valor[i]="NULL";}

		///////////////////////////////////////////////////////////////////////////////////
		//Modificacion para valores NULL - 28/10/24

		if(valor[i].compare("NULL")==0){

			if (i == (cantidad - 1)) {
				query1 = query1 + "`" + campo[i] + "` = " + valor[i];
			}
			else {
				query1 = query1 + "`" + campo[i] + "` = " + valor[i] + ",";
			}
		}

		else{
			if (i == (cantidad - 1)) {
				query1 = query1 + "`" + campo[i] + "` = '" + valor[i] + "'";
			}
			else {
				query1 = query1 + "`" + campo[i] + "` = '" + valor[i] + "',";
			}
		}

		/* Codigo Original
		if (i == (cantidad - 1)) {
			query1 = query1 + "`" + campo[i] + "` = '" + valor[i] + "'";
		}
		else {
			query1 = query1 + "`" + campo[i] + "` = '" + valor[i] + "',";
		}
		*/
	}

	query1 = query1 + " WHERE `" + campo[indice1] + "` = '" + valor[indice1] + "'";
	query1 = query1 + " AND `" + campo[indice2] + "` = '" + valor[indice2] + "'";
	query1 = query1 + " AND `" + campo[indice3] + "` = '" + valor[indice3] + "'";


	char arrSQL[1024] = { 0 };

	//arrSQL = new char[query1.size()+1];

	//cout << "query1:" << query1 << endl;

	memset(arrSQL, 0, 1024);
	direccionpuerto = "tcp://" + (db->direccion) + ":" + cast_int_to_string(db->puerto);
	sprintf(arrSQL, query1.c_str());
	imprimir_mensaje("actualizacion valores: " + query1, negro);
	//printf(arrSQL); printf("\n");

	try {
		sql::Driver* driver;
		sql::Connection* con;
		sql::Statement* stmt;


		/* Crea una conexion con la base de datos externa */
		driver = get_driver_instance();
		con = driver->connect(direccionpuerto.c_str(), (db->usuario).c_str(), (db->clave).c_str());
		int timeout1 = timeout_mysql;
		con->setClientOption("MYSQL_OPT_CONNECT_TIMEOUT", &timeout1);
		/* Conecta con la base de datos */
		con->setSchema((db->nombre).c_str());
		stmt = con->createStatement();
		resultado = stmt->execute(arrSQL);

		//cout << "resultado:" << resultado << endl;


		delete stmt;
		delete con;
		//delete arrSQL;

		res_int = resultado ? -1 : 0;

		//cout << "resultado entero:" << res_int << endl;

		return (res_int);

	}
	catch (sql::SQLException& e) {

		std::string mensajeError = "ERROR: SQLException in " + std::string(__FILE__) +
                               "(" + std::string(__FUNCTION__) + ") on line " + std::to_string(__LINE__) +
                               ".# Error description: " + e.what() +
                               " (MySQL error code: " + std::to_string(e.getErrorCode()) +
                               ", SQLState: " + e.getSQLState() + " )";

    	std::cout << mensajeError << std::endl;
    	logger_loadconfig.registrarError(mensajeError);
		return -1;
	}

}




// actualiza los valores de cualquier tabla
// el resultado es 0 si todo ok, y -1 si hay algun error


int actualizar_valor_condicion(struct stdatabase* db, string tabla, string campo, string valor, string condicion) {


	if (db->direccion.compare(GLOBAL_dbext_direccion) == 0) {
		if (GLOBAL_habilitar_acceso_externo == 0) {
			imprimir_mensaje("en la configuracion configdb esta desactivado el acceso a internet", negro);
			return 0;
		}
		if (GLOBAL_ACCESO_EXTERNO == false) {
			imprimir_mensaje("se desactivo momentaneamente el acceso al servidor mysql externo", negro);
			return 0;
		}
		else {
			if (chequear_internet(GLOBAL_direccionip_dns)) {
				;
			}
			else {
				imprimir_mensaje("no hay internet, no se puede alcanzar el servidor externo", negro);
				return 0;
			}
		}

	}

	bool resultado = false;
	int res_int = 0;
	string direccionpuerto;
	string SQL_STR = "";
	string query1 = "UPDATE `" + tabla + "` SET " + "`" + campo + "` = " + "'" + valor + "'";

	query1 = query1 + " WHERE " + condicion;

	char arrSQL[1024] = { 0 };

	//arrSQL = new char[query1.size()+1];

	//cout << "query1:" << query1 << endl;

	memset(arrSQL, 0, 1024);
	direccionpuerto = "tcp://" + (db->direccion) + ":" + cast_int_to_string(db->puerto);
	sprintf(arrSQL, query1.c_str());
	imprimir_mensaje("actualizacion valores: " + query1, negro);
	//printf(arrSQL); printf("\n");

	try {
		sql::Driver* driver;
		sql::Connection* con;
		sql::Statement* stmt;


		/* Crea una conexion con la base de datos externa */
		driver = get_driver_instance();
		con = driver->connect(direccionpuerto.c_str(), (db->usuario).c_str(), (db->clave).c_str());
		int timeout1 = timeout_mysql;
		con->setClientOption("MYSQL_OPT_CONNECT_TIMEOUT", &timeout1);
		/* Conecta con la base de datos */
		con->setSchema((db->nombre).c_str());
		stmt = con->createStatement();
		resultado = stmt->execute(arrSQL);

		//cout << "resultado:" << resultado << endl;


		delete stmt;
		delete con;
		//delete arrSQL;

		res_int = resultado ? -1 : 0;

		//cout << "resultado entero:" << res_int << endl;

		return (res_int);

	}
	catch (sql::SQLException& e) {

		std::string mensajeError = "ERROR: SQLException in " + std::string(__FILE__) +
                               "(" + std::string(__FUNCTION__) + ") on line " + std::to_string(__LINE__) +
                               ".# Error description: " + e.what() +
                               " (MySQL error code: " + std::to_string(e.getErrorCode()) +
                               ", SQLState: " + e.getSQLState() + " )";

    	std::cout << mensajeError << std::endl;
    	logger_loadconfig.registrarError(mensajeError);
		return -1;
	}

}



// inserta los valores de cualquier tabla
// el resultado es 0 si todo ok, y -1 si hay algun error
int insertar_valor(struct stdatabase* db, string tabla, string* campo, string* valor, int cantidad) {


	if (db->direccion.compare(GLOBAL_dbext_direccion) == 0) {
		if (GLOBAL_habilitar_acceso_externo == 0) {
			imprimir_mensaje("en la configuracion configdb esta desactivado el acceso a internet", negro);
			return 0;
		}
		if (GLOBAL_ACCESO_EXTERNO == false) {
			imprimir_mensaje("se desactivo momentaneamente el acceso al servidor mysql externo", negro);
			return 0;
		}
		else {
			if (chequear_internet(GLOBAL_direccionip_dns)) {
				;
			}
			else {
				imprimir_mensaje("no hay internet, no se puede alcanzar el servidor externo", negro);
				return 0;
			}
		}

	}

	bool resultado = false;
	int res_int = 0;
	string direccionpuerto;
	string SQL_STR = "";
	string query1 = "INSERT INTO `" + tabla + "` (";
	for (int i = 0; i < cantidad; i++) {
		if (i == (cantidad - 1)) {
			query1 = query1 + "`" + campo[i] + "`" + ") VALUES (";
		}
		else {
			query1 = query1 + "`" + campo[i] + "`" + ",";
		}

	}

	for (int i = 0; i < cantidad; i++) {

		//Correccion: 25/10/24
		//Valor NaN a NULL.
		if (valor[i].compare("-nan")==0){valor[i]="NULL";}
		///////////////////////////////////////////////////
		
		if (!(valor[i].compare("NULL") == 0)) {

			if (i == (cantidad - 1)) {
				query1 = query1 + "'" + valor[i] + "'" + ")";
			}
			else {
				query1 = query1 + "'" + valor[i] + "'" + ",";
			}
		}

		else {

			if (i == (cantidad - 1)) {
				query1 = query1 + valor[i] + ")";
			}
			else {
				query1 = query1 + valor[i] + ",";
			}

		}

	}

	char arrSQL[1024] = { 0 };

	//arrSQL = new char[query1.size()+1];

	imprimir_mensaje("insercion valor: " + query1, negro);
	memset(arrSQL, 0, 1024);
	direccionpuerto = "tcp://" + (db->direccion) + ":" + cast_int_to_string(db->puerto);
	sprintf(arrSQL, query1.c_str());

	try {
		sql::Driver* driver;
		sql::Connection* con;
		sql::Statement* stmt;


		/* Crea una conexion con la base de datos externa */
		driver = get_driver_instance();
		con = driver->connect(direccionpuerto.c_str(), (db->usuario).c_str(), (db->clave).c_str());
		int timeout1 = timeout_mysql;
		con->setClientOption("MYSQL_OPT_CONNECT_TIMEOUT", &timeout1);
		/* Conecta con la base de datos */
		con->setSchema((db->nombre).c_str());
		stmt = con->createStatement();
		resultado = stmt->execute(arrSQL);

		//cout << "resultado:" << resultado << endl;


		delete stmt;
		delete con;
		//delete arrSQL;

		res_int = resultado ? -1 : 0;

		//cout << "resultado entero:" << res_int << endl;

		return (res_int);

	}
	catch (sql::SQLException& e) {
		GLOBAL_ACCESO_EXTERNO = false;
		imprimir_mensaje("falla en mysql: ", rojo);
		imprimir_mensaje("se deshabilito lectura mysql externo por " + cast_int_to_string(GLOBAL_periodo_reseteo_mysqlexterno) + " seg ", amarillo);
		std::string mensajeError = "ERROR: SQLException in " + std::string(__FILE__) +
                               "(" + std::string(__FUNCTION__) + ") on line " + std::to_string(__LINE__) +
                               ".# Error description: " + e.what() +
                               " (MySQL error code: " + std::to_string(e.getErrorCode()) +
                               ", SQLState: " + e.getSQLState() + " )";

    	std::cout << mensajeError << std::endl;
    	logger_loadconfig.registrarError(mensajeError);
		return -1;
	}


}


// inserta los valores de cualquier tabla
// el resultado es 0 si todo ok, y -1 si hay algun error
int insertar_inteligente(struct stdatabase* db, string tabla, int indice, string* campo, string* valor, int cantidad) {
	int res = -1;
	int rescmd = -1;
	int resfinal = 0;
	res = -1;
	rescmd = -1;
	res = buscarentabla(db, tabla, campo[indice], valor[indice]);
	if (res > 0) {
		//actualiza
		rescmd = actualizar_valor(db, tabla, indice, &campo[0], &valor[0], cantidad);
	}
	else {
		//inserta
		rescmd = insertar_valor(db, tabla, &campo[0], &valor[0], cantidad);
	}
	if (rescmd == 0) {
		resfinal = 0;
	}


	return resfinal;

}

// inserta los valores de cualquier tabla
// el resultado es 0 si todo ok, y -1 si hay algun error
int insertar_inteligente3(struct stdatabase* db, string tabla, int indice1, int indice2, int indice3, string* campo, string* valor, int cantidad) {
	int res = -1;
	int rescmd = -1;
	int resfinal = 0;
	res = -1;
	rescmd = -1;
	res = buscarentabla3(db, tabla, campo[indice1], campo[indice2], campo[indice3], valor[indice1], valor[indice2], valor[indice3]);
	if (res > 0) {
		//actualiza
		rescmd = actualizar_valor3(db, tabla, indice1, indice2, indice3, &campo[0], &valor[0], cantidad);
	}
	else {
		//inserta
		rescmd = insertar_valor(db, tabla, &campo[0], &valor[0], cantidad);
	}
	if (rescmd == 0) {
		resfinal = 0;
	}


	return resfinal;

}



// devuelve >0 si todo ok y la cantidad de registros copiados
//-1 si hay falla
int replicaciontablaselectiva(struct stdatabase* dborigen, string tablaorigen, struct stdatabase* dbdestino, string tabladestino, string* campos, int cantidadcampos, int limitefilastablaorigen)

{

	int ind = 0;  // contador de comandos a ejecutar
	string cadena;
	string direccionpuerto_origen;
	direccionpuerto_origen = "tcp://" + dborigen->direccion + ":" + cast_int_to_string(dborigen->puerto);
	string cadenasql_origen = "SELECT ";
	try {
		sql::Driver* driver;
		sql::Connection* con;
		sql::Statement* stmt;
		sql::ResultSet* res;


		for (int j = 0; j < cantidadcampos; j++) {
			if (j == cantidadcampos - 1) {
				cadenasql_origen += "`" + campos[j] + "`";
			}
			else {
				cadenasql_origen += "`" + campos[j] + "`,";
			}

		}

		cadenasql_origen += " FROM `" + tablaorigen + "` LIMIT " + cast_int_to_string(limitefilastablaorigen);

		//cout << cadenasql_origen << endl;

		/* Crea una conexion */
		driver = get_driver_instance();
		con = driver->connect(direccionpuerto_origen.c_str(), (dborigen->usuario).c_str(), (dborigen->clave).c_str());
		int timeout1 = 3;
		con->setClientOption("MYSQL_OPT_CONNECT_TIMEOUT", &timeout1);
		con->setClientOption("MYSQL_OPT_READ_TIMEOUT", &timeout1);
		string* valorescampos = new string[cantidadcampos];

		/* Conecta con la base de datos */
		con->setSchema((dborigen->nombre).c_str());

		stmt = con->createStatement();

		res = stmt->executeQuery(cadenasql_origen.c_str());

		while (res->next() && (ind < limitefilastablaorigen)) {

			for (int i = 0; i < cantidadcampos; i++) {
				valorescampos[i] = res->getString(i + 1);     //	  	  
			}

			insertar_inteligente(dbdestino, tabladestino, 0, &campos[0], &valorescampos[0], cantidadcampos);

			ind++;

		}
		delete res;
		delete stmt;
		delete con;
		return (ind);

	}
	catch (sql::SQLException& e) {
		std::string mensajeError = "ERROR: SQLException in " + std::string(__FILE__) +
                               "(" + std::string(__FUNCTION__) + ") on line " + std::to_string(__LINE__) +
                               ".# Error description: " + e.what() +
                               " (MySQL error code: " + std::to_string(e.getErrorCode()) +
                               ", SQLState: " + e.getSQLState() + " )";

    	std::cout << mensajeError << std::endl;
    	logger_loadconfig.registrarError(mensajeError);
		return -1;
	}


}



// leer tabla de definicion de eventos
int actualizar_defeventos(struct stdatabase* db) {

	if (db->direccion.compare(GLOBAL_dbext_direccion) == 0) {
		if (GLOBAL_habilitar_acceso_externo == 0) {
			imprimir_mensaje("en la configuracion configdb esta desactivado el acceso a internet", negro);
			return 0;
		}
		if (GLOBAL_ACCESO_EXTERNO == false) {
			imprimir_mensaje("se desactivo momentaneamente el acceso al servidor mysql externo", negro);
			return 0;
		}
		else {
			if (chequear_internet(GLOBAL_direccionip_dns)) {
				;
			}
			else {
				imprimir_mensaje("no hay internet, no se puede alcanzar el servidor externo", negro);
				return 0;
			}
		}

	}

	int id = 0;
	int ind = 0;  // contador de comandos a ejecutar
	string activar = "";
	string desactivar = "";
	string direccionpuerto;
	string SQL_STR = "";
	string descripcion = "";
	int habilitar = 0;
	int LOCAL_contador_defeventos;

	string query1 = "SELECT COUNT(`id`) as contador FROM `tbdefeventos` WHERE habilitar = 1";
	string query2 = "SELECT * FROM `tbdefeventos` WHERE habilitar = 1";
	char arrSQL[1024] = { 0 };
	memset(arrSQL, 0, 1024);
	direccionpuerto = "tcp://" + (db->direccion) + ":" + cast_int_to_string(db->puerto);
	sprintf(arrSQL, query1.c_str());

	//cout << "direccion:" << direccionpuerto << endl;
	//cout << "base de datos:" << (db->nombre) << endl;

	try {
		sql::Driver* driver;
		sql::Connection* con;
		sql::Statement* stmt;
		sql::ResultSet* res;

		/* Crea una conexion con la base de datos externa */
		driver = get_driver_instance();
		con = driver->connect(direccionpuerto.c_str(), (db->usuario).c_str(), (db->clave).c_str());
		int timeout1 = timeout_mysql;
		con->setClientOption("MYSQL_OPT_CONNECT_TIMEOUT", &timeout1);
		/* Conecta con la base de datos */
		con->setSchema((db->nombre).c_str());
		stmt = con->createStatement();
		res = stmt->executeQuery(arrSQL);

		while (res->next()) {
			LOCAL_contador_defeventos = res->getInt("contador");
		}
		imprimir_mensaje("cantidad de eventos definidos: " + cast_int_to_string(LOCAL_contador_defeventos));
		memset(arrSQL, 0, 1024);
		sprintf(arrSQL, query2.c_str());
		res = stmt->executeQuery(arrSQL);
		ind = 0;


		// se reserva memoria
		defeventos = new cls_defeventos[LOCAL_contador_defeventos + 1];

		while (res->next() && (ind < LOCAL_contador_defeventos)) {
			defeventos[ind].id = res->getInt("id"); //
			defeventos[ind].nombredispositivo = res->getString("nombredispositivo");
			defeventos[ind].tag = res->getString("tag");
			defeventos[ind].bit1 = res->getInt("bit1");
			defeventos[ind].activado = res->getString("activado");
			defeventos[ind].desactivado = res->getString("desactivado");

			ind++;
		}

		GLOBAL_contador_defeventos = ind;

		delete res;
		delete stmt;
		delete con;


		return (ind);

	}
	catch (sql::SQLException& e) {
		GLOBAL_ACCESO_EXTERNO = false;
		imprimir_mensaje("falla en mysql: ", rojo);
		imprimir_mensaje("se deshabilito lectura mysql externo por " + cast_int_to_string(GLOBAL_periodo_reseteo_mysqlexterno) + " seg ", amarillo);
		std::string mensajeError = "ERROR: SQLException in " + std::string(__FILE__) +
                               "(" + std::string(__FUNCTION__) + ") on line " + std::to_string(__LINE__) +
                               ".# Error description: " + e.what() +
                               " (MySQL error code: " + std::to_string(e.getErrorCode()) +
                               ", SQLState: " + e.getSQLState() + " )";

    	std::cout << mensajeError << std::endl;
    	logger_loadconfig.registrarError(mensajeError);
		return -1;
	}

}


// leer tabla de numeros de telefono para sms
int actualizar_smslist(struct stdatabase* db) {

	if (db->direccion.compare(GLOBAL_dbext_direccion) == 0) {
		if (GLOBAL_habilitar_acceso_externo == 0) {
			imprimir_mensaje("en la configuracion configdb esta desactivado el acceso a internet", negro);
			return 0;
		}
		if (GLOBAL_ACCESO_EXTERNO == false) {
			imprimir_mensaje("se desactivo momentaneamente el acceso al servidor mysql externo", negro);
			return 0;
		}
		else {
			if (chequear_internet(GLOBAL_direccionip_dns)) {
				;
			}
			else {
				imprimir_mensaje("no hay internet, no se puede alcanzar el servidor externo", negro);
				return 0;
			}
		}

	}


	int id = 0;
	int ind = 0;  // contador de comandos a ejecutar
	string direccionpuerto;
	string SQL_STR = "";
	string descripcion = "";
	int habilitar = 0;

	string query1 = "SELECT COUNT(`id`) as contador FROM `tbsms`";
	string query2 = "SELECT * FROM `tbsms`";
	char arrSQL[1024] = { 0 };
	memset(arrSQL, 0, 1024);
	direccionpuerto = "tcp://" + (db->direccion) + ":" + cast_int_to_string(db->puerto);
	sprintf(arrSQL, query1.c_str());

	//cout << "direccion:" << direccionpuerto << endl;
	//cout << "base de datos:" << (db -> nombre) << endl;

	try {
		sql::Driver* driver;
		sql::Connection* con;
		sql::Statement* stmt;
		sql::ResultSet* res;

		/* Crea una conexion con la base de datos externa */
		driver = get_driver_instance();
		con = driver->connect(direccionpuerto.c_str(), (db->usuario).c_str(), (db->clave).c_str());
		int timeout1 = timeout_mysql;
		con->setClientOption("MYSQL_OPT_CONNECT_TIMEOUT", &timeout1);
		/* Conecta con la base de datos */
		con->setSchema((db->nombre).c_str());
		stmt = con->createStatement();
		res = stmt->executeQuery(arrSQL);

		while (res->next()) {
			GLOBAL_contador_numerossms = res->getInt("contador");
		}
		memset(arrSQL, 0, 1024);
		sprintf(arrSQL, query2.c_str());
		res = stmt->executeQuery(arrSQL);
		ind = 0;


		// se reserva memoria
		smslist = new cls_sms[GLOBAL_contador_numerossms + 1];

		while (res->next() && (ind < GLOBAL_contador_numerossms)) {

			if (&smslist[ind]) {
				smslist[ind].clsid = res->getInt("id"); //
				smslist[ind].clsnumero = res->getString("numero");
				smslist[ind].clsasunto = res->getString("asunto");
				smslist[ind].clsurl_modem = res->getString("url_modem");
				smslist[ind].clsparametros_rest_api = res->getString("parametros_rest_api");
				smslist[ind].clsmetodohtml = res->getString("metodohtml"); //
			}

			ind++;
		}

		GLOBAL_contador_numerossms = ind;

		delete res;
		delete stmt;
		delete con;


		return (ind);

	}
	catch (sql::SQLException& e) {
		GLOBAL_ACCESO_EXTERNO = false;
		imprimir_mensaje("falla en mysql: ", rojo);
		imprimir_mensaje("se deshabilito lectura mysql externo por " + cast_int_to_string(GLOBAL_periodo_reseteo_mysqlexterno) + " seg ", amarillo);
		std::string mensajeError = "ERROR: SQLException in " + std::string(__FILE__) +
                               "(" + std::string(__FUNCTION__) + ") on line " + std::to_string(__LINE__) +
                               ".# Error description: " + e.what() +
                               " (MySQL error code: " + std::to_string(e.getErrorCode()) +
                               ", SQLState: " + e.getSQLState() + " )";

    	std::cout << mensajeError << std::endl;
    	logger_loadconfig.registrarError(mensajeError);
		return -1;
	}
}

	void liberarmemoria(void) {
	
		delete[] equipo_mbtcp;
		delete[] equipo_mbserial;
		delete[] equipo_rs232;
		delete[] equipo_rs232_distintos;
		return;
	
	}



int insertar_eventos_tbhistoalarmas(string descripcion, string actdesact) {
	string arr_campos1[13];
	string arr_valores1[13];
	int res_insertardblocal;
	int id = 0;
	arr_campos1[0] = "id";
	arr_campos1[1] = "id_alarma";
	arr_campos1[2] = "fechatiempo";
	arr_campos1[3] = "tiempo";
	arr_campos1[4] = "tipo";
	arr_campos1[5] = "descripcion";
	arr_campos1[6] = "valor";
	arr_campos1[7] = "accion";
	arr_campos1[8] = "condicion";
	arr_campos1[9] = "concentrador";
	arr_campos1[10] = "dispositivo";
	arr_campos1[11] = "tag";
	arr_campos1[12] = "reconocida";
	arr_valores1[0] = "NULL";
	arr_valores1[1] = cast_int_to_string(id);
	arr_valores1[2] = cast_time_to_datetime(time(NULL));
	arr_valores1[3] = cast_time_to_datetime(time(NULL));
	arr_valores1[4] = actdesact;
	arr_valores1[5] = descripcion;
	arr_valores1[6] = "0";
	arr_valores1[7] = "";
	arr_valores1[8] = "" ;
	arr_valores1[9] = GLOBAL_nombre_concentrador;
	arr_valores1[10] = "";
	arr_valores1[11] = "";
	arr_valores1[12] = "0";
	res_insertardblocal = insertar_valor(&stdblocal, "tbhistoalarmas", &arr_campos1[0], &arr_valores1[0], 13);	


return 0;
}
