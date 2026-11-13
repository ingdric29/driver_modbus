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
#include "include/sql_session.h"
#include "include/replication_cursor.h"
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

namespace {
bool is_external_target(const std::string& host, int port, const std::string& schema) {
    const bool external = host == GLOBAL_dbext_direccion && port == cast_string_to_int(GLOBAL_dbext_puerto) && schema == GLOBAL_dbext_nombre;
    const bool local = host == GLOBAL_db_direccion && port == cast_string_to_int(GLOBAL_db_puerto) && schema == GLOBAL_db_nombre;
    return external && !local;
}
bool database_allowed(const stdatabase* db) {
    return db != &stdbexterna || (GLOBAL_habilitar_acceso_externo && GLOBAL_ACCESO_EXTERNO);
}
std::shared_ptr<sql::Connection> database_session(const stdatabase* db) {
    return sql_connection("tcp://" + db->direccion + ":" + std::to_string(db->puerto), db->usuario, db->clave, db->nombre);
}
void database_failure(const std::exception& error) {
    ++DriverMetrics::instance().sql_failures;
    sql_sessions().invalidate();
    logger_loadconfig.registrarError(std::string("SQL operation failed: ") + error.what());
}
int update_fields(stdatabase* db, const std::string& table, string* fields, string* values, int count,
    const std::vector<int>& keys, const std::string& condition = "") {
    if (!database_allowed(db) || count <= 0) return -1;
    try {
        std::string query = "UPDATE " + sql_identifier(table) + " SET ";
        for (int i = 0; i < count; ++i) query += (i ? "," : "") + sql_identifier(fields[i]) + "=?";
        query += " WHERE ";
        if (!condition.empty()) query += condition; // Internal SQL expression, never a bound value.
        else for (size_t i = 0; i < keys.size(); ++i) {
            if (keys[i] < 0 || keys[i] >= count) return -1;
            query += (i ? " AND " : "") + sql_identifier(fields[keys[i]]) + "=?";
        }
        auto connection = database_session(db);
        std::unique_ptr<sql::PreparedStatement> statement(connection->prepareStatement(query));
        int parameter = 1;
        for (int i = 0; i < count; ++i) bind_legacy(*statement, parameter++, values[i]);
        for (int key : keys) bind_legacy(*statement, parameter++, values[key]);
        statement->executeUpdate();
        return 0;
    } catch (const std::exception& error) { database_failure(error); return -1; }
}
int find_fields(stdatabase* db, const std::string& table, const std::vector<std::string>& fields, const std::vector<std::string>& values) {
    if (!database_allowed(db)) return -1;
    try {
        std::string query = "SELECT 1 FROM " + sql_identifier(table) + " WHERE ";
        for (size_t i = 0; i < fields.size(); ++i) query += (i ? " AND " : "") + sql_identifier(fields[i]) + "=?";
        query += " LIMIT 1";
        auto connection = database_session(db);
        std::unique_ptr<sql::PreparedStatement> statement(connection->prepareStatement(query));
        for (size_t i = 0; i < values.size(); ++i) statement->setString(i + 1, values[i]);
        std::unique_ptr<sql::ResultSet> result(statement->executeQuery());
        return result->next() ? 1 : 0;
    } catch (const std::exception& error) { database_failure(error); return -1; }
}
}

int loadconfigdb()
{
	int contador_campos = 0;
	INIReader reader(obtenerDirectorio("configdb.cfg"));

	if (reader.ParseError() != 0) {
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

		GLOBAL_picos_tags = reader.Get(*sectionsIt, "picos_tags", "");
        GLOBAL_replicacion_inicial_id = reader.GetInteger(*sectionsIt, "replicacion_inicial_id", -1);
        G_modbus_timeout = reader.GetInteger(*sectionsIt, "modbus_timeout_us", 90000);
        if (G_modbus_timeout < 1000 || G_modbus_timeout > 3000000) return -1;
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

    try {
        for (double period : {GLOBAL_periodo_muestreo_tbejecutar, GLOBAL_periodoext_muestreo_tbejecutar,
            GLOBAL_periodo_muestreo_tbcambios, GLOBAL_periodo_revision_alarmas,
            GLOBAL_periodo_revision_eventos, GLOBAL_periodo_minimo_escritura_tbdispositivos_sta})
            PeriodicDeadline::interval(period);
        if (!std::isfinite(GLOBAL_periodo_minimo_escritura_tbvalores) || GLOBAL_periodo_minimo_escritura_tbvalores < 0) return -1;
    } catch (const std::invalid_argument& error) { logger_loadconfig.registrarError(error.what()); return -1; }
	return 1;
}



// lee la tabla tbcambios
int lee_tbcambios(void) {

	int id = 0;
	int cont_tbregistros;
	int cont_tbdispositivos;
	int cont_tbdefequipos;
	int cont_cambio_tablacom = GLOBAL_cont_cambio_tablacom;
	int cont_tbdefalarmas;
	int cambio = 0;
	string direccionpuerto;
	direccionpuerto = "tcp://" + GLOBAL_db_direccion + ":" + GLOBAL_db_puerto;
	GLOBAL_contador_equipos = 0;

	try {
		sql::Driver* driver;
		std::shared_ptr<sql::Connection> con;
		std::unique_ptr<sql::Statement> stmt;
		std::unique_ptr<sql::ResultSet> res;

		/* Crea una conexion */
		driver = get_driver_instance();
		con = sql_connection(direccionpuerto, GLOBAL_db_usuario, GLOBAL_db_clave, GLOBAL_db_nombre); //GLOBAL_usuario_db.c_str(), GLOBAL_clave_db.c_str());
		/* Conecta con la base de datos */
		con->setSchema(GLOBAL_db_nombre.c_str());

		stmt.reset(con->createStatement());
		res.reset(stmt->executeQuery("SELECT * FROM `tbcambios` WHERE `id`=1"));

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




		return cambio;

	}
	catch (sql::SQLException& e) {
        sql_sessions().invalidate();
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
		std::shared_ptr<sql::Connection> con;
		std::unique_ptr<sql::Statement> stmt;
		std::unique_ptr<sql::ResultSet> res;

		/* Crea una conexion */
		driver = get_driver_instance();
		con = sql_connection(direccionpuerto, GLOBAL_db_usuario, GLOBAL_db_clave, GLOBAL_db_nombre); //GLOBAL_usuario_db.c_str(), GLOBAL_clave_db.c_str());
		/* Conecta con la base de datos */
		con->setSchema(GLOBAL_db_nombre.c_str());


		stmt.reset(con->createStatement());
		res.reset(stmt->executeQuery(query.c_str()));

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


		res.reset(stmt->executeQuery(query.c_str()));

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
				equipo_mbtcp[ind].tag = res->getString("tag");
                equipo_mbtcp[ind].preservar_picos = ("," + GLOBAL_picos_tags + ",").find("," + equipo_mbtcp[ind].nombre + "/" + equipo_mbtcp[ind].tag + ",") != std::string::npos; //
				equipo_mbtcp[ind].protocolo = res->getString("protocolo"); //
				equipo_mbtcp[ind].registro = res->getInt("registro"); //
				equipo_mbtcp[ind].funcion = res->getString("funcion"); //
				equipo_mbtcp[ind].interpretacion = res->getString("interpretacion"); //
				cadena = res->getString("escalamiento"); //
				equipo_mbtcp[ind].escalamiento = getValfloat(cadena);


				equipo_mbtcp[ind].periodo = cast_string_to_double(res->getString("periodo"));
                try { PeriodicDeadline::interval(equipo_mbtcp[ind].periodo); }
                catch (const std::invalid_argument& e) { logger_loadconfig.registrarError(std::string("Invalid tag period: ")+e.what());    return -1; } //
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
				equipo_mbserial[ind].tag = res->getString("tag");
                equipo_mbserial[ind].preservar_picos = ("," + GLOBAL_picos_tags + ",").find("," + equipo_mbserial[ind].nombre + "/" + equipo_mbserial[ind].tag + ",") != std::string::npos; //
				equipo_mbserial[ind].protocolo = res->getString("protocolo"); //
				equipo_mbserial[ind].registro = res->getInt("registro"); //
				equipo_mbserial[ind].funcion = res->getString("funcion"); //
				equipo_mbserial[ind].interpretacion = res->getString("interpretacion"); //
				cadena = res->getString("escalamiento"); //
				equipo_mbserial[ind].escalamiento = getValfloat(cadena);
				equipo_mbserial[ind].periodo = cast_string_to_double(res->getString("periodo"));
                try { PeriodicDeadline::interval(equipo_mbserial[ind].periodo); }
                catch (const std::invalid_argument& e) { logger_loadconfig.registrarError(std::string("Invalid tag period: ")+e.what());    return -1; } //
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
				equipo_rs232[ind].tag = res->getString("tag");
                equipo_rs232[ind].preservar_picos = ("," + GLOBAL_picos_tags + ",").find("," + equipo_rs232[ind].nombre + "/" + equipo_rs232[ind].tag + ",") != std::string::npos; //
				equipo_rs232[ind].protocolo = res->getString("protocolo"); //
				equipo_rs232[ind].registro = res->getInt("registro"); //
				equipo_rs232[ind].funcion = res->getString("funcion"); //
				equipo_rs232[ind].interpretacion = res->getString("interpretacion"); //
				cadena = res->getString("escalamiento"); //
				equipo_rs232[ind].escalamiento = getValfloat(cadena);
				equipo_rs232[ind].html_id = tratamientocadena(res->getString("html_id")); // delimitador inicial
				equipo_rs232[ind].html_final = tratamientocadena(res->getString("html_final")); // delimitador final
				equipo_rs232[ind].periodo = cast_string_to_double(res->getString("periodo"));
                try { PeriodicDeadline::interval(equipo_rs232[ind].periodo); }
                catch (const std::invalid_argument& e) { logger_loadconfig.registrarError(std::string("Invalid tag period: ")+e.what());    return -1; } //
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
						equipo_mbtcp[i].state_mutex = equipo_mbtcp[j].state_mutex;
                        equipo_mbtcp[i].suspension_logged = equipo_mbtcp[j].suspension_logged;
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
						equipo_mbserial[i].state_mutex = equipo_mbserial[j].state_mutex;
                        equipo_mbserial[i].suspension_logged = equipo_mbserial[j].suspension_logged;
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
						equipo_rs232[i].state_mutex = equipo_rs232[j].state_mutex;
                        equipo_rs232[i].suspension_logged = equipo_rs232[j].suspension_logged;
						equipo_rs232[i].pcontador_errores = equipo_rs232[j].pcontador_errores;
						equipo_rs232[i].pcontador_errores_acumulados = equipo_rs232[j].pcontador_errores_acumulados;
						equipo_rs232[i].ptiempotranscurridoerrores = equipo_rs232[j].ptiempotranscurridoerrores;
						equipo_rs232[i].ptiemporeset_cuenta_errores = equipo_rs232[j].ptiemporeset_cuenta_errores;
						equipo_rs232[i].pultima_llamada_leer = equipo_rs232[j].pultima_llamada_leer;
						equipo_rs232[i].buffer_mutex = equipo_rs232[j].buffer_mutex;
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



		return 1;

	}
	catch (sql::SQLException& e) {
        sql_sessions().invalidate();
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




int insertardb(time_t fechatiempo, string tag, string valorstr, float valorfloat, int dispositivo, string rastro, string nombre_concentrador, string nombre_dispositivo, string direccion_db, int puerto_db, string nombre_db, string usuario_db, string clave_db, string dbtabla) {
    try {
        SampleValue sample; sample.acquired = fechatiempo; sample.text = valorstr; sample.value = valorfloat;
        auto connection = sql_connection("tcp://" + direccion_db + ":" + std::to_string(puerto_db), usuario_db, clave_db, nombre_db);
        store_sample_batch(connection, dbtabla, {sample}, tag, dispositivo, rastro, nombre_concentrador, nombre_dispositivo);
        return 1;
    } catch (const std::exception& error) { database_failure(error); return -1; }
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

	if (is_external_target(direccion_db, puerto_db, nombre_db)) {
		if (GLOBAL_habilitar_acceso_externo == 0) {
			imprimir_mensaje("en la configuracion configdb esta desactivado el acceso a internet", negro);
			return 0;
		}
		if (GLOBAL_ACCESO_EXTERNO == false) {
			imprimir_mensaje("se desactivo momentaneamente el acceso al servidor mysql externo", negro);
			return 0;
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
		std::shared_ptr<sql::Connection> con;
		std::unique_ptr<sql::Statement> stmt;
		std::unique_ptr<sql::ResultSet> res;

		/* Crea una conexion */
		driver = get_driver_instance();
		con = sql_connection(direccionpuerto, usuario_db, clave_db, nombre_db); //GLOBAL_usuario_db.c_str(), GLOBAL_clave_db.c_str());
		/* Conecta con la base de datos */
		con->setSchema(nombre_db.c_str());

		stmt.reset(con->createStatement());
		sprintf(arrSQL, "SELECT * FROM `tbejecutar` WHERE `concentrador` = '%s' ORDER BY `id` DESC LIMIT 1", GLOBAL_nombre_concentrador.c_str());
		res.reset(stmt->executeQuery(arrSQL));

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



		res_ejecutar = ejecutar_tbcmd;
		return (int)res_update;

	}

	catch (sql::SQLException& e) {
        sql_sessions().invalidate();
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
    std::lock_guard<std::recursive_mutex> commands(GLOBAL_command_mutex);

	if (is_external_target(direccion_db, puerto_db, nombre_db)) {

		if (GLOBAL_habilitar_acceso_externo == 0) {
			imprimir_mensaje("en la configuracion configdb esta desactivado el acceso a internet", negro);
			return 0;
		}

		if (GLOBAL_ACCESO_EXTERNO == false) {
			imprimir_mensaje("se desactivo momentaneamente el acceso al servidor mysql externo", negro);
			return 0;
		}


	}

	int pending_count = 0;
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
		std::shared_ptr<sql::Connection> con;
		std::unique_ptr<sql::Statement> stmt;
		std::unique_ptr<sql::ResultSet> res;

		/* Crea una conexion */
		driver = get_driver_instance();
		con = sql_connection(direccionpuerto, usuario_db, clave_db, nombre_db);
		/* Conecta con la base de datos */
		con->setSchema(nombre_db.c_str());

		stmt.reset(con->createStatement());
		res.reset(stmt->executeQuery(cadenaquerycmd3));
		while (res->next()) {
			pending_count = res->getInt("contador");
		}

		std::unique_ptr<cls_cmd[]> pending(new cls_cmd[pending_count + 1]());
		memset(cadenaquerycmd3, 0, 2000);
		sprintf(cadenaquerycmd3, "SELECT * FROM (%s) as vistacmd where `concentrador` = '%s' ORDER BY `id` ASC", cadenaquerycmd.c_str(), GLOBAL_nombre_concentrador.c_str());
		res.reset(stmt->executeQuery(cadenaquerycmd3));
		//cout << cadenaquerycmd3 << endl;
		while (res->next() && (ind < 128)) {
			pending[ind].id = res->getInt("id");
			pending[ind].nombre = res->getString("nombre"); //
			pending[ind].direccion_ip = res->getString("direccion_ip"); //
			pending[ind].puerto = res->getInt("puerto"); //
			pending[ind].direccion_esclavo = res->getInt("direccion_esclavo"); //
			pending[ind].interface1 = res->getString("interface"); //
			pending[ind].velocidad = res->getInt("velocidad"); //
			pending[ind].paridad = res->getString("paridad"); //
			pending[ind].bitsparada = res->getInt("bitsparada"); //
			pending[ind].equipo = res->getString("equipo"); //

			pending[ind].usuario = res->getString("usuario"); //
			pending[ind].password = res->getString("password"); //
			pending[ind].habilitar = ((res->getInt("habilitar")) != 0); //
			pending[ind].protocolo = res->getString("protocolo"); //
			pending[ind].registro = res->getInt("registro"); //
			pending[ind].clsvalor = res->getInt("valor"); //
			pending[ind].datetime = res->getString("datetime"); //
			pending[ind].hab_ejecutar = ((res->getInt("hab_ejecutar")) != 0); //
			pending[ind].lote = res->getString("lote"); //
			ind++;

			//cout << "\t valores: " << cadena << "\t" << intTime1 << endl; 	

			//   cout << res->getString(1) << endl;
		}



		delete[] cmd;
		cmd = pending.release();
		GLOBAL_contador_cmd = ind;
		return (ind);

	}
	catch (sql::SQLException& e) {
        sql_sessions().invalidate();
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

	if (is_external_target(direccion_db, puerto_db, nombre_db)) {
		if (GLOBAL_habilitar_acceso_externo == 0) {
			imprimir_mensaje("en la configuracion configdb esta desactivado el acceso a internet", negro);
			return 0;
		}

		if (GLOBAL_ACCESO_EXTERNO == false) {
			imprimir_mensaje("se desactivo momentaneamente el acceso al servidor mysql externo", negro);
			return 0;
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
		std::shared_ptr<sql::Connection> con;
		std::unique_ptr<sql::Statement> stmt;


		/* Crea una conexion */
		driver = get_driver_instance();
		con = sql_connection(direccionpuerto, usuario_db, clave_db, nombre_db); //GLOBAL_usuario_db.c_str(), GLOBAL_clave_db.c_str());
		/* Conecta con la base de datos */
		con->setSchema(nombre_db.c_str());

		stmt.reset(con->createStatement());

		tiempoactual = time(NULL);
		valor_tiempo_str = cast_time_to_datetime(tiempoactual);

		SQL_STR = "UPDATE `%s`.`tbcmd` SET `cmd_datetime` = '%s' WHERE `tbcmd`.`id` = %i;";
		sprintf(arrSQL, SQL_STR.c_str(), nombre_db.c_str(), valor_tiempo_str.c_str(), id);
		res_update = stmt->execute(arrSQL);



		return (int)res_update;

	}

	catch (sql::SQLException& e) {
        sql_sessions().invalidate();
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
    if (is_external_target(direccion_db, puerto_db, nombre_db)) {
        if (GLOBAL_habilitar_acceso_externo == 0) {
            imprimir_mensaje("en la configuracion configdb esta desactivado el acceso a internet", negro);
            return 0;
        }
        if (GLOBAL_ACCESO_EXTERNO == false) {
            imprimir_mensaje("se desactivo momentaneamente el acceso al servidor mysql externo", negro);
            return 0;
        }
        else {

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
        auto con = sql_connection(direccionpuerto, usuario_db, clave_db, nombre_db);
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
        sql_sessions().invalidate();
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
int sincronizar_tablas(stdatabase* local, stdatabase* remote, string table, int limit) {
    if (!GLOBAL_habilitar_acceso_externo || !GLOBAL_ACCESO_EXTERNO) return 0;
    if (limit <= 0 || limit > 10000) return -1;
    if (local->direccion == remote->direccion && local->puerto == remote->puerto && local->nombre == remote->nombre) return -1;
    try {
        const auto identity = local->direccion + ":" + std::to_string(local->puerto) + "/" + local->nombre + "->"
            + remote->direccion + ":" + std::to_string(remote->puerto) + "/" + remote->nombre + "/" + table + "/" + GLOBAL_nombre_concentrador;
        const auto path = obtenerDirectorio("log") + "/" + replication_cursor_name(identity);
        uint64_t cursor = 0;
        auto destination = database_session(remote);
        if (!read_replication_cursor(path, identity, cursor)) {
            if (GLOBAL_replicacion_inicial_id >= 0) cursor = GLOBAL_replicacion_inicial_id;
            else {
                std::unique_ptr<sql::PreparedStatement> check(destination->prepareStatement("SELECT 1 FROM " + sql_identifier(table) + " WHERE concentrador=? LIMIT 1"));
                check->setString(1, GLOBAL_nombre_concentrador);
                std::unique_ptr<sql::ResultSet> existing(check->executeQuery());
                if (existing->next()) throw std::runtime_error("replication destination already has data; configure replicacion_inicial_id or restore checkpoint");
            }
        }
        auto source = database_session(local);
        std::unique_ptr<sql::PreparedStatement> select(source->prepareStatement("SELECT * FROM " + sql_identifier(table) + " WHERE concentrador=? AND id>? ORDER BY id ASC LIMIT ?"));
        select->setString(1, GLOBAL_nombre_concentrador); select->setUInt64(2, cursor); select->setInt(3, limit);
        std::unique_ptr<sql::ResultSet> rows(select->executeQuery());
        SqlTransaction transaction(destination);
        std::unique_ptr<sql::PreparedStatement> insert(destination->prepareStatement("INSERT INTO " + sql_identifier(table)
            + " (timedate,fechatiempo,tag,valorstr,valorfloat,dispositivo,rastro,concentrador,nombredispositivo) VALUES (?,?,?,?,?,?,?,?,?)"));
        int copied = 0; uint64_t confirmed = cursor;
        while (rows->next()) {
            insert->setString(1, rows->getString("timedate")); insert->setString(2, rows->getString("fechatiempo"));
            insert->setString(3, rows->getString("tag"));
            if (rows->isNull("valorstr")) insert->setNull(4, sql::DataType::VARCHAR); else insert->setString(4, rows->getString("valorstr"));
            if (rows->isNull("valorfloat")) insert->setNull(5, sql::DataType::DOUBLE); else insert->setDouble(5, rows->getDouble("valorfloat"));
            insert->setInt(6, rows->getInt("dispositivo")); insert->setString(7, rows->getString("rastro"));
            insert->setString(8, rows->getString("concentrador")); insert->setString(9, rows->getString("nombredispositivo"));
            if (insert->executeUpdate() != 1) throw std::runtime_error("replication insert was not acknowledged");
            confirmed = rows->getUInt64("id"); ++copied;
        }
        transaction.commit();
        if (copied) write_replication_cursor(path, identity, confirmed);
        DriverMetrics::instance().replicated_samples += copied;
        return copied;
    } catch (const std::exception& error) { database_failure(error); return -1; }
}




// leer tabla de definicion de alarmas
int actualizar_defalarmas(struct stdatabase* db) {

	if (db == &stdbexterna) {
		if (GLOBAL_habilitar_acceso_externo == 0) {
			imprimir_mensaje("en la configuracion configdb esta desactivado el acceso a internet", negro);
			return 0;
		}
		if (GLOBAL_ACCESO_EXTERNO == false) {
			imprimir_mensaje("se desactivo momentaneamente el acceso al servidor mysql externo", negro);
			return 0;
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
		std::shared_ptr<sql::Connection> con;
		std::unique_ptr<sql::Statement> stmt;
		std::unique_ptr<sql::ResultSet> res;

		/* Crea una conexion con la base de datos externa */
		driver = get_driver_instance();
		con = database_session(db);
		/* Conecta con la base de datos */
		con->setSchema((db->nombre).c_str());
		stmt.reset(con->createStatement());
		res.reset(stmt->executeQuery(arrSQL));

		while (res->next()) {
			GLOBAL_contador_defalarmas = res->getInt("contador");
		}
		imprimir_mensaje("cantidad de alarmas definidas: " + cast_int_to_string(GLOBAL_contador_defalarmas));
		memset(arrSQL, 0, 1024);
		sprintf(arrSQL, query2.c_str());
		res.reset(stmt->executeQuery(arrSQL));
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






		return (ind);

	}
	catch (sql::SQLException& e) {
        sql_sessions().invalidate();
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

	if (db == &stdbexterna) {
		if (GLOBAL_habilitar_acceso_externo == 0) {
			imprimir_mensaje("en la configuracion configdb esta desactivado el acceso a internet", negro);
			return 0;
		}
		if (GLOBAL_ACCESO_EXTERNO == false) {
			imprimir_mensaje("se desactivo momentaneamente el acceso al servidor mysql externo", negro);
			return 0;
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
		std::shared_ptr<sql::Connection> con;
		std::unique_ptr<sql::Statement> stmt;
		std::unique_ptr<sql::ResultSet> res;

		/* Crea una conexion con la base de datos externa */
		driver = get_driver_instance();
		con = database_session(db);
		/* Conecta con la base de datos */
		con->setSchema((db->nombre).c_str());
		stmt.reset(con->createStatement());
		res.reset(stmt->executeQuery(arrSQL));

		while (res->next()) {
			GLOBAL_contador_directorio = res->getInt("contador");
		}
		memset(arrSQL, 0, 1024);
		sprintf(arrSQL, query2.c_str());
		res.reset(stmt->executeQuery(arrSQL));
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






		return (ind);

	}
	catch (sql::SQLException& e) {
        sql_sessions().invalidate();
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
int buscarentabla(stdatabase* db, string tabla, string campo, string valor) {
    return find_fields(db, tabla, {campo}, {valor});
}


// revuelve 0 si no consigue
//devuelve 1 si consigue el item buscado
int buscarentabla3(stdatabase* db, string tabla, string campo1, string campo2, string campo3, string valor1, string valor2, string valor3) {
    return find_fields(db, tabla, {campo1, campo2, campo3}, {valor1, valor2, valor3});
}


// actualiza los valores de cualquier tabla
// el resultado es 0 si todo ok, y -1 si hay algun error


int actualizar_valor(stdatabase* db, string tabla, int indice, string* campo, string* valor, int cantidad) {
    return update_fields(db, tabla, campo, valor, cantidad, {indice});
}


// actualiza los valores de cualquier tabla
// el resultado es 0 si todo ok, y -1 si hay algun error


int actualizar_valor3(stdatabase* db, string tabla, int indice1, int indice2, int indice3, string* campo, string* valor, int cantidad) {
    return update_fields(db, tabla, campo, valor, cantidad, {indice1, indice2, indice3});
}




// actualiza los valores de cualquier tabla
// el resultado es 0 si todo ok, y -1 si hay algun error


int actualizar_valor_condicion(stdatabase* db, string tabla, string campo, string valor, string condicion) {
    return update_fields(db, tabla, &campo, &valor, 1, {}, condicion);
}



// inserta los valores de cualquier tabla
// el resultado es 0 si todo ok, y -1 si hay algun error
int insertar_valor(stdatabase* db, string tabla, string* campo, string* valor, int cantidad) {
    if (!database_allowed(db) || cantidad <= 0) return -1;
    try {
        std::string query = "INSERT INTO " + sql_identifier(tabla) + " (", placeholders;
        for (int i = 0; i < cantidad; ++i) {
            query += (i ? "," : "") + sql_identifier(campo[i]);
            placeholders += i ? ",?" : "?";
        }
        query += ") VALUES (" + placeholders + ")";
        auto connection = database_session(db);
        std::unique_ptr<sql::PreparedStatement> statement(connection->prepareStatement(query));
        for (int i = 0; i < cantidad; ++i) bind_legacy(*statement, i + 1, valor[i]);
        return statement->executeUpdate() == 1 ? 0 : -1;
    } catch (const std::exception& error) { database_failure(error); return -1; }
}


// inserta los valores de cualquier tabla
// el resultado es 0 si todo ok, y -1 si hay algun error
int insertar_inteligente(struct stdatabase* db, string tabla, int indice, string* campo, string* valor, int cantidad) {
	int res = -1;
	int rescmd = -1;
	int resfinal = -1;
	res = -1;
	rescmd = -1;
	res = buscarentabla(db, tabla, campo[indice], valor[indice]);
	if (res < 0) return -1;
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
	int resfinal = -1;
	res = -1;
	rescmd = -1;
	res = buscarentabla3(db, tabla, campo[indice1], campo[indice2], campo[indice3], valor[indice1], valor[indice2], valor[indice3]);
	if (res < 0) return -1;
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
		std::shared_ptr<sql::Connection> con;
		std::unique_ptr<sql::Statement> stmt;
		std::unique_ptr<sql::ResultSet> res;


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
		con = database_session(dborigen);
		int timeout1 = 3;
		con->setClientOption("MYSQL_OPT_READ_TIMEOUT", &timeout1);
		string* valorescampos = new string[cantidadcampos];

		/* Conecta con la base de datos */
		con->setSchema((dborigen->nombre).c_str());

		stmt.reset(con->createStatement());

		res.reset(stmt->executeQuery(cadenasql_origen.c_str()));

		while (res->next() && (ind < limitefilastablaorigen)) {

			for (int i = 0; i < cantidadcampos; i++) {
				valorescampos[i] = res->getString(i + 1);     //	  	  
			}

			insertar_inteligente(dbdestino, tabladestino, 0, &campos[0], &valorescampos[0], cantidadcampos);

			ind++;

		}



		return (ind);

	}
	catch (sql::SQLException& e) {
        sql_sessions().invalidate();
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

	if (db == &stdbexterna) {
		if (GLOBAL_habilitar_acceso_externo == 0) {
			imprimir_mensaje("en la configuracion configdb esta desactivado el acceso a internet", negro);
			return 0;
		}
		if (GLOBAL_ACCESO_EXTERNO == false) {
			imprimir_mensaje("se desactivo momentaneamente el acceso al servidor mysql externo", negro);
			return 0;
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
		std::shared_ptr<sql::Connection> con;
		std::unique_ptr<sql::Statement> stmt;
		std::unique_ptr<sql::ResultSet> res;

		/* Crea una conexion con la base de datos externa */
		driver = get_driver_instance();
		con = database_session(db);
		/* Conecta con la base de datos */
		con->setSchema((db->nombre).c_str());
		stmt.reset(con->createStatement());
		res.reset(stmt->executeQuery(arrSQL));

		while (res->next()) {
			LOCAL_contador_defeventos = res->getInt("contador");
		}
		imprimir_mensaje("cantidad de eventos definidos: " + cast_int_to_string(LOCAL_contador_defeventos));
		memset(arrSQL, 0, 1024);
		sprintf(arrSQL, query2.c_str());
		res.reset(stmt->executeQuery(arrSQL));
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






		return (ind);

	}
	catch (sql::SQLException& e) {
        sql_sessions().invalidate();
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

	if (db == &stdbexterna) {
		if (GLOBAL_habilitar_acceso_externo == 0) {
			imprimir_mensaje("en la configuracion configdb esta desactivado el acceso a internet", negro);
			return 0;
		}
		if (GLOBAL_ACCESO_EXTERNO == false) {
			imprimir_mensaje("se desactivo momentaneamente el acceso al servidor mysql externo", negro);
			return 0;
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
		std::shared_ptr<sql::Connection> con;
		std::unique_ptr<sql::Statement> stmt;
		std::unique_ptr<sql::ResultSet> res;

		/* Crea una conexion con la base de datos externa */
		driver = get_driver_instance();
		con = database_session(db);
		/* Conecta con la base de datos */
		con->setSchema((db->nombre).c_str());
		stmt.reset(con->createStatement());
		res.reset(stmt->executeQuery(arrSQL));

		while (res->next()) {
			GLOBAL_contador_numerossms = res->getInt("contador");
		}
		memset(arrSQL, 0, 1024);
		sprintf(arrSQL, query2.c_str());
		res.reset(stmt->executeQuery(arrSQL));
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






		return (ind);

	}
	catch (sql::SQLException& e) {
        sql_sessions().invalidate();
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
    // Called only after all workers have joined (also handles partial startup).
    delete[] equipo_mbtcp; equipo_mbtcp = nullptr;
    delete[] equipo_mbserial; equipo_mbserial = nullptr;
    delete[] equipo_rs232; equipo_rs232 = nullptr;
    delete[] equipo_rs232_distintos; equipo_rs232_distintos = nullptr;
    delete[] hilosseriales; hilosseriales = nullptr;
    delete[] defalarmas; defalarmas = nullptr;
    delete[] defeventos; defeventos = nullptr;
    delete[] cmd; cmd = nullptr;
    delete[] emaillist; emaillist = nullptr;
    delete[] smslist; smslist = nullptr;
    GLOBAL_contador_equipos_mbtcp = GLOBAL_contador_equipos_mbserial = 0;
    GLOBAL_contador_equipos_rs232 = GLOBAL_contador_equipos_rs232_distintos = 0;
    GLOBAL_contador_defalarmas = GLOBAL_contador_cmd = 0;
    GLOBAL_contador_defeventos = -1;
    GLOBAL_contador_directorio = GLOBAL_contador_numerossms = 0;
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
