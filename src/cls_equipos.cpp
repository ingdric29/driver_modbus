/*
programa dedarrollado por ing. Alejandro diomede - Automatroni - email: diomede3009@gmail.com
Define la estructura de datos llamada Equipos - tcp y serial
Modificación hecha por: Ing. Javier Flores 12/11/24
 + Ubicacion de los archivos cabecera cambiada. 
 + Añadida la funcion Log que permite hacer un registro de los eventos importantes

*/

#include "include/globales.h"
#include "include/cls_equipos.h"
#include "include/conversiones.h"
#include <errno.h>
#include <modbus.h>
#include <vector>
#include <ctype.h>
#include <sstream>

//////////////////////////////
#include "include/cls_logger.h"
#include <cppconn/connection.h>
//////////////////////////////

/////////////////////////////
#include <thread>
#include <chrono>
#include <cmath>
////////////////////////////


#ifdef _WIN32
#include <Windows.h>
#else
#include <unistd.h>
#endif

#include "include/rs232.h"

using namespace std;

string cls_equipos::static_clsnombre_mbserial = "";
time_t cls_equipos::static_clsultimalectura_mbserial = time(NULL);
int* cls_equipos::static_clsregistros_mbserial = new int[20];
int* cls_equipos::static_clsdatosleidos_mbserial = new int[20];

string cls_equipos::static_clsnombre_mbtcp = "";
time_t cls_equipos::static_clsultimalectura_mbtcp = time(NULL);
int* cls_equipos::static_clsregistros_mbtcp = new int[20];
int* cls_equipos::static_clsdatosleidos_mbtcp = new int[20];

/////////////////////////////////////
Logger logger;
bool GLOBAL_habilitar_log=true;
////////////////////////////////////
int actualizarValores() {
    std::string query = "UPDATE `tbinstantaneos` SET `valorstr` = ?, `valorfloat` = ? WHERE `tbinstantaneos`.tag = ?";
    std::string direccionpuerto = "tcp://127.0.0.1:3306";
    std::string usuario = "automatroni";
    std::string password = "automa2424";
    std::string schema = "dbdevlog";

    try {
        // Crear una conexión con la base de datos
        sql::Driver* driver = get_driver_instance();
        std::unique_ptr<sql::Connection> con(driver->connect(direccionpuerto, usuario, password));
        con->setSchema(schema);

        // Crear una consulta preparada
        std::unique_ptr<sql::PreparedStatement> pstmt(con->prepareStatement(query));
        pstmt->setString(1, "1");  // valorstr
        pstmt->setDouble(2, 1.0);  // valorfloat
        pstmt->setString(3, "wAlarmas04");  // tag

        // Ejecutar la consulta
        int rowsAffected = pstmt->executeUpdate();
        std::cout << "Filas afectadas: " << rowsAffected << std::endl;

        return (rowsAffected > 0) ? 0 : -1;
    }
    catch (sql::SQLException& e) {
        std::string mensajeError = "ERROR: SQLException in " + std::string(__FILE__) +
            "(" + std::string(__FUNCTION__) + ") on line " + std::to_string(__LINE__) +
            ".# Error description: " + e.what() +
            " (MySQL error code: " + std::to_string(e.getErrorCode()) +
            ", SQLState: " + e.getSQLState() + " )";

        std::cout << mensajeError << std::endl;
        return -1;
    }
}


cls_equipos::cls_equipos() {

	ultimo_muestreo = 0; // ultimo muestreo para la lecturas y alarmas
	ultimo_insertar = 0; // ultimo comando insertar en base de datos tbvalores
	ultimo_insertar_ex = 0; // ultimo comando insertar en bd externa
	ultima_escritura_tbinstantaneo = 0;
	html_final = "</div>";
	html_separador = "-";
	html_indice = 0;
	contador_errores = 0;
	contador_errores_acumulados = 0;
	pcontador_errores = &contador_errores;
	pcontador_errores_acumulados = &contador_errores_acumulados;

	ptiemporeset_cuenta_errores = &tiemporeset_cuenta_errores;
	ptiempotranscurridoerrores = &tiempotranscurridoerrores;
	pultima_llamada_leer = &ultima_llamada_leer;
	clsvalorstr = "0.0";
	clsvalor = 0;
	clsvalorfloat = 0.0;
	estadocomunicacion = false;
	pestadocomunicacion = &estadocomunicacion;
	cuenta_errores_maximo = GLOBAL_maxima_cuenta_errores;
	tiemporeset_cuenta_errores = GLOBAL_tiempo_restablecimiento_comunicacion;
	numerodebits = 8;
	controldeflujo = 0; // para www.teuniz.net/RS-232/. 0 es sin control de flujo
	tamanoventana = 32;
	tamanobuffer = (int) tamanoventana * 2;
	buf = (unsigned char*) new char[tamanobuffer]; 
	p_n_buf = &n_buf;
	fuecambiado = false;
	fueescritodb = true;
	resultadoleer_rc = -1;
};

void cls_equipos::incrementarerrores() {
	if (*pcontador_errores < 32000) {
		*pcontador_errores = *pcontador_errores + 1;
	}
	if (*pcontador_errores_acumulados < 32000) {
		*pcontador_errores_acumulados = *pcontador_errores_acumulados + 1;
	}

}

// metodo leer variables modbus
int cls_equipos::leer() {
	string mensajedeerror = "";
	

	resultadoleer_rc = -1;
	if (*pcontador_errores > cuenta_errores_maximo) {
		*pestadocomunicacion = false;
		*ptiempotranscurridoerrores = *ptiempotranscurridoerrores + difftime(time(NULL), *pultima_llamada_leer);
	}
	else {
		*ptiempotranscurridoerrores = 0.0;
	}
	*pultima_llamada_leer = time(NULL);

	if (*ptiempotranscurridoerrores > tiemporeset_cuenta_errores) {
		*pcontador_errores = cuenta_errores_maximo / 2;
		mensajedeerror = "se activa dispositivo " + nombre + " para verificar ";
		mensajedeerror += "si hay conexion ";
		imprimir_mensaje_importante(mensajedeerror, verde);
		logger.registrarError("INFO: Connection verification. Device "+nombre+" activated.");
		GLOBAL_habilitar_log = true;
	}

	if (*pcontador_errores > cuenta_errores_maximo) {
		mensajedeerror = "se suspende dispositivo " + nombre + " por exceso de error y ";
		mensajedeerror += "se reanuda dentro de " + cast_double_to_string(tiemporeset_cuenta_errores - *ptiempotranscurridoerrores) + " segundos";
		imprimir_mensaje_importante(mensajedeerror, rojo);//Loggin
		if(GLOBAL_habilitar_log){logger.registrarError("INFO: Excess errors. Device "+nombre+" suspended.");GLOBAL_habilitar_log=false;actualizarValores();}
		return -2; 
	}
	imprimir_mensaje("leer: " + tag, negro);
	
	if ((protocolo.compare("MBTCP") == 0)) {
		leer_mbtcp();
	}
	else if (protocolo.compare("MBSERIAL") == 0) {
		leer_mbserial();
	}
	else if (protocolo.compare("HTML_S9") == 0) {
		leer_htmls9();
	}
	else if (protocolo.compare("RS232") == 0) {
		//cout << " lectura rs232 ";
		leer_rs232();
		
	};


	if (resultadoleer_rc > 0) {
		ultimo_muestreo = time(NULL);
	}
	else {
		ultimo_muestreo = time(NULL) - (periodo / 2);
	}

	if (*pcontador_errores < (cuenta_errores_maximo / 2)) {
		*pestadocomunicacion = true;
	}
	else {
		*pestadocomunicacion = false;
	}


	return resultadoleer_rc;
}

//---------------------------------------------------------------------
/* void cls_equipos::leer_mbtcp(void) {
		modbus_t* ctx_mbtcp = NULL;
		bool conectadocondispositivo = true;
		string mensajedeerror = "";
		int intento = 0;
		int intentosmax = 3;
		uint16_t datos[20]; //reserva suficiente espacio para 10 registros long o 20 reg word
		uint8_t datos8b[4];
		float valor_float;
		uint32_t valor_long;
		uint16_t valor;
		char buffer[4];
		stringstream ssdatos;
		int rc = -1;

		static_clsnombre_mbtcp = nombre;
		ctx_mbtcp = modbus_new_tcp(direccion_ip.c_str(), puerto);
		rastro = nombre + ";" + direccion_ip + ":" + cast_int_to_string(puerto) + ";" + tag + ";" + "idslave=" + cast_int_to_string(direccion_esclavo) + ";reg=" + cast_int_to_string(registro);

		if (ctx_mbtcp == NULL) {
			fprintf(stderr, "=falla en iniciar libmodbus\n");
			logger.registrarError("DEBUG: libmodbus fail start in: leer_mbtcp() : " + std::string(__FILE__));
			conectadocondispositivo = false;
			resultado_leer = false;
			rc = -1;
		}
		if (direccion_esclavo == 0) direccion_esclavo = 1;
		modbus_set_slave(ctx_mbtcp, direccion_esclavo);

		if (modbus_connect(ctx_mbtcp) == -1) {
			string smsmodbus = string(modbus_strerror(errno));
			mensajedeerror = "fallo en modbus errno= " + smsmodbus + " ";
			mensajedeerror += rastro;
			imprimir_mensaje(mensajedeerror, rojo); //Loggin
			logger.registrarError("WARN: Modbus TCP read fail."+smsmodbus+" "+rastro);
			modbus_free(ctx_mbtcp);
			conectadocondispositivo = false;
			resultado_leer = false;
			rc = -1;
		}
		// lee los holding registers
		rc = -1;
		intento = 0;
		if (cantidad1 > 20) cantidad1 = 20;
		//modbus_set_response_timeout(ctx_mbtcp, 0, 200000);

		while ((intento < intentosmax) && (rc == -1) && (conectadocondispositivo == true)) { //while 1
			  //cout << "la funcion es: " << funcion << endl;
			  //cout << "la interpretacion es: " << interpretacion << endl;
			intento++;
			//cout << "leer:ln115" << endl;	
			memset(datos, 0, 20 * sizeof(uint16_t));
			memset(datos8b, 0, 4 * sizeof(uint8_t));
			memset(buffer, 0, 4 * sizeof(char));
			valor_float = 0.0;
			valor_long = 0;
			valor = 0;
			if (intento > 1) { esperar(0.1); };

			if (funcion.compare("hr") == 0) {

				if (interpretacion.compare("float32abcd") == 0) {
					rc = modbus_read_registers(ctx_mbtcp, registro, 2, datos);
					valor_float = cast_get_float_abcd(datos);
					sprintf(buffer, "%f", valor_float);
					//arr_valores[i] = string(buffer);
					//cout << "1*";
				}
				else if (interpretacion.compare("float32badc") == 0)
				{
					rc = modbus_read_registers(ctx_mbtcp, registro, 2, datos);
					valor_float = cast_get_float_badc(datos);
					sprintf(buffer, "%f", valor_float);
					//arr_valores[i] = string(buffer);
					//cout << "2*";				
				}
				else if (interpretacion.compare("float32cdab") == 0)
				{
					rc = modbus_read_registers(ctx_mbtcp, registro, 2, datos);
					valor_float = cast_get_float_cdab(datos);
					sprintf(buffer, "%f", valor_float);
					//arr_valores[i] = string(buffer);
					//cout << "2*";				
				}
				else if (interpretacion.compare("float32dcba") == 0)
				{

					rc = modbus_read_registers(ctx_mbtcp, registro, 2, datos);
					//cout << "dato_0:" << datos[0] << "   " << "dato_1:" << datos[1] << endl;
					valor_float = cast_get_float_dcba(datos);
					sprintf(buffer, "%f", valor_float);
					//cout << (rc != -1)? string(buffer) : "0.0";
					imprimir_mensaje("**"+cast_float_to_string(valor_float), negro);
					//cout << "***" << valor_float << endl;

				}
				else if (interpretacion.compare("float32fp") == 0) // solo para los PM factor de potencia
				{
					rc = modbus_read_registers(ctx_mbtcp, registro, 2, datos);
					valor_float = cast_get_float_dcba(datos);
					valor_float = convert_float_fp(valor_float);
					sprintf(buffer, "%f", valor_float);
					//arr_valores[i] = (rc != -1)? string(buffer) : "0.0";
					//cout << "2*";				
				}
				else if (interpretacion.compare("longle") == 0)
				{
					rc = modbus_read_registers(ctx_mbtcp, registro, 2, datos);
					valor_long = datos[0] + (datos[1] << 16);
					sprintf(buffer, "%f", valor_long);
					//arr_valores[i] = string(buffer);
					//cout << "3*";								
				}

				else if (interpretacion.compare("longle64") == 0)
				{
					rc = modbus_read_registers(ctx_mbtcp, registro, 4, datos);
					valor_long = datos[0] + (datos[1] << 16) + (datos[2] << 32) + (datos[3] << 48);
					sprintf(buffer, "%i", valor_long);
					//arr_valores[i] = string(buffer);
					//cout << "3*";								
				}

				else if (interpretacion.compare("longbe") == 0)
				{
					rc = modbus_read_registers(ctx_mbtcp, registro, 2, datos);
					valor_long = datos[1] + (datos[0] << 16);
					//cout << "valor long be:" << valor_long << endl;
					sprintf(buffer, "%i", valor_long);
					//cout << "valor buffer:" << buffer << endl;
					//arr_valores[i] = string(buffer);
					//cout << "4*";								
				}

				else if (interpretacion.compare("word") == 0)
				{
					rc = modbus_read_registers(ctx_mbtcp, registro, cantidad1, datos);

					if ((bit1 >= 0) && (bit1 <= 15)) {
						valor = obtenervalorbit(datos[0], bit1);
					}
					else {
						valor = datos[0];
					}
					sprintf(buffer, "%i", valor);
					//arr_valores[i] = (rc != -1)? string(buffer) : "0";
					//cout << "interpretado como word registro:" << registro << "valor: " << valor << endl;								
				}
				//
				else
				{
					cout << "En \"" << nombre << "\" tipo no def!!: " << interpretacion << endl;


				}
			}

			if (funcion.compare("rr") == 0) {
				if (interpretacion.compare("float32abcd") == 0) {
					rc = modbus_read_input_registers(ctx_mbtcp, registro, 2, datos);
					valor_float = cast_get_float_abcd(datos);
					sprintf(buffer, "%f", valor_float);
					//arr_valores[i] = string(buffer);				
				}
				else if (interpretacion.compare("float32badc") == 0)
				{
					rc = modbus_read_input_registers(ctx_mbtcp, registro, 2, datos);
					valor_float = cast_get_float_badc(datos);
					sprintf(buffer, "%f", valor_float);
					//arr_valores[i] = string(buffer);				
				}
				//float32cdab
				else if (interpretacion.compare("float32cdab") == 0)
				{
					rc = modbus_read_input_registers(ctx_mbtcp, registro, 2, datos);
					valor_float = cast_get_float_cdab(datos);
					sprintf(buffer, "%f", valor_float);
					//arr_valores[i] = string(buffer);				
				}
				//float32dcba
				else if (interpretacion.compare("float32dcba") == 0)
				{
					rc = modbus_read_input_registers(ctx_mbtcp, registro, 2, datos);
					valor_float = cast_get_float_dcba(datos);
					sprintf(buffer, "%f", valor_float);
					//arr_valores[i] = string(buffer);				
				}
				else if (interpretacion.compare("longle") == 0)
				{
					rc = modbus_read_input_registers(ctx_mbtcp, registro, 2, datos);
					valor_long = datos[0] + (datos[1] << 16);
					sprintf(buffer, "%i", valor_long);
					//arr_valores[i] = string(buffer);				
				}

				else if (interpretacion.compare("longbe") == 0)
				{
					rc = modbus_read_input_registers(ctx_mbtcp, registro, 2, datos);
					valor_long = datos[1] + (datos[0] << 16);
					//cout << "valor long be:" << valor_long << endl;

					sprintf(buffer, "%i", valor_long);
					//arr_valores[i] = string(buffer);
					//cout << "valor buffer:" << buffer << endl;
				}
				else if (interpretacion.compare("word") == 0)
				{
					rc = modbus_read_input_registers(ctx_mbtcp, registro, cantidad1, datos);

					if ((bit1 >= 0) && (bit1 <= 15)) {
						valor = obtenervalorbit(datos[0], bit1);
					}
					else {
						valor = datos[0];
					}
					sprintf(buffer, "%i", valor);
					//arr_valores[i] = string(buffer);				
				}
				else
				{
					cout << "En \"" << nombre << "\" tipo no def!!: " << interpretacion << endl;


				}
			} // fin del if del read register

			if (funcion.compare("cs") == 0) {
				rc = modbus_read_bits(ctx_mbtcp, registro, 1, datos8b);
				sprintf(buffer, "%i", datos8b[0]);
				memcpy(datos, datos8b, 4);
			}


			if (funcion.compare("ir") == 0) {
				rc = modbus_read_input_bits(ctx_mbtcp, registro, 1, datos8b);
				sprintf(buffer, "%i", datos8b[0]);
				memcpy(datos, datos8b, 4);

			}

		} // fin del while 1

		if (rc != -1) {
		setvalorstring(cast_arrchar_to_string(buffer)); // esta funcion actualiza el valor
		resultado_leer = true;
		*pcontador_errores = 0;
		ssdatos << "dato[0]: " << datos[0] << " dato[1]: " << datos[1];
		imprimir_mensaje(ssdatos.str(), negro);
		imprimir_mensaje("valor float: " + cast_float_to_string(atof(buffer)), negro);
		imprimir_mensaje("valor float escalado: " + cast_float_to_string(clsvalorfloat), negro);
		//setvalorfloat
		memcpy(&static_clsdatosleidos_mbtcp[0], &datos[0], 20);
				for (int offset1 = 0; offset1 < 20; offset1++) {
					if (offset1 > cantidad1) {
						static_clsregistros_mbtcp[offset1] = -1;
					}
					else {
						static_clsregistros_mbtcp[offset1] = registro + offset1;
					}

		}
	
		//leido del tag

		//clsvalor = atoi(buffer);
		//clsvalorfloat = atof(buffer) * escalamiento;
		//clsvalorstr = cast_float_to_string(clsvalorfloat);
		}
		else {
			resultado_leer = false;
			incrementarerrores();
			descripcionerror = rastro + " - error modbus";//Loggin
			logger.registrarError("WARN: Modbus TCP read fail. "+rastro);
			imprimir_mensaje(descripcionerror, rojo);
		}

		if (conectadocondispositivo) {
			modbus_close(ctx_mbtcp);
			modbus_free(ctx_mbtcp);
		};

		resultadoleer_rc = rc;
	
};

 */
//---------------------------------------------------------------------
//leer_mbtcp() con nueva interpretacion de valores Word con signo

void cls_equipos::leer_mbtcp(void) {
    modbus_t* ctx_mbtcp = NULL;
    bool conectadocondispositivo = true;
    string mensajedeerror = "";
    int intento = 0;
    int intentosmax = 3;
    uint16_t datos[20]; //reserva suficiente espacio para 10 registros long o 20 reg word
    uint8_t datos8b[4];
    float valor_float;
    uint32_t valor_long;
    uint16_t valor;
    char buffer[32];
    stringstream ssdatos;
    int rc = -1;

    static_clsnombre_mbtcp = nombre;
    ctx_mbtcp = modbus_new_tcp(direccion_ip.c_str(), puerto);
    rastro = nombre + ";" + direccion_ip + ":" + cast_int_to_string(puerto) + ";" + tag + ";" + "idslave=" + cast_int_to_string(direccion_esclavo) + ";reg=" + cast_int_to_string(registro);

    if (ctx_mbtcp == NULL) {
        fprintf(stderr, "=falla en iniciar libmodbus\n");
        logger.registrarError("DEBUG: libmodbus fail start in: leer_mbtcp() : " + std::string(__FILE__));
        conectadocondispositivo = false;
        resultado_leer = false;
        rc = -1;
    }

    if (direccion_esclavo == 0) direccion_esclavo = 1;
    modbus_set_slave(ctx_mbtcp, direccion_esclavo);

    if (modbus_connect(ctx_mbtcp) == -1) {
        string smsmodbus = string(modbus_strerror(errno));
        mensajedeerror = "fallo en modbus errno= " + smsmodbus + " ";
        mensajedeerror += rastro;
        imprimir_mensaje(mensajedeerror, rojo); //Loggin
        logger.registrarError("WARN: Modbus TCP read fail."+smsmodbus+" "+rastro);
        modbus_free(ctx_mbtcp);
        conectadocondispositivo = false;
        resultado_leer = false;
        rc = -1;
    }

    // lee los holding registers
    rc = -1;
    intento = 0;
    if (cantidad1 > 20) cantidad1 = 20;
    //modbus_set_response_timeout(ctx_mbtcp, 0, 200000);

    while ((intento < intentosmax) && (rc == -1) && (conectadocondispositivo == true)) {
        //cout << "la funcion es: " << funcion << endl;
        //cout << "la interpretacion es: " << interpretacion << endl;

        intento++;

        memset(datos, 0, 20 * sizeof(uint16_t));
        memset(datos8b, 0, 4 * sizeof(uint8_t));
        memset(buffer, 0, sizeof(buffer));

        valor_float = 0.0;
        valor_long = 0;
        valor = 0;

        if (intento > 1) { esperar(0.1); };

        // ============================================================
        // HOLDING REGISTERS (hr)
        // ============================================================

        if (funcion.compare("hr") == 0) {

            if (interpretacion.compare("float32abcd") == 0) {
                rc = modbus_read_registers(ctx_mbtcp, registro, 2, datos);
                valor_float = cast_get_float_abcd(datos);
                sprintf(buffer, "%f", valor_float);
            }

            else if (interpretacion.compare("float32badc") == 0)
            {
                rc = modbus_read_registers(ctx_mbtcp, registro, 2, datos);
                valor_float = cast_get_float_badc(datos);
                sprintf(buffer, "%f", valor_float);
            }

            else if (interpretacion.compare("float32cdab") == 0)
            {
                rc = modbus_read_registers(ctx_mbtcp, registro, 2, datos);
                valor_float = cast_get_float_cdab(datos);
                sprintf(buffer, "%f", valor_float);
            }

            else if (interpretacion.compare("float32dcba") == 0)
            {
                rc = modbus_read_registers(ctx_mbtcp, registro, 2, datos);
                valor_float = cast_get_float_dcba(datos);
                sprintf(buffer, "%f", valor_float);
                imprimir_mensaje("**"+cast_float_to_string(valor_float), negro);
            }

            else if (interpretacion.compare("float32fp") == 0)
            {
                // solo para los PM factor de potencia
                rc = modbus_read_registers(ctx_mbtcp, registro, 2, datos);
                valor_float = cast_get_float_dcba(datos);
                valor_float = convert_float_fp(valor_float);
                sprintf(buffer, "%f", valor_float);
            }

            else if (interpretacion.compare("longle") == 0)
            {
                rc = modbus_read_registers(ctx_mbtcp, registro, 2, datos);
                valor_long = datos[0] + (datos[1] << 16);
                sprintf(buffer, "%f", valor_long);
            }

            else if (interpretacion.compare("longle64") == 0)
            {
                rc = modbus_read_registers(ctx_mbtcp, registro, 4, datos);
                valor_long = datos[0] + (datos[1] << 16) + (datos[2] << 32) + (datos[3] << 48);
                sprintf(buffer, "%i", valor_long);
            }

            else if (interpretacion.compare("longbe") == 0)
            {
                rc = modbus_read_registers(ctx_mbtcp, registro, 2, datos);
                valor_long = datos[1] + (datos[0] << 16);
                sprintf(buffer, "%i", valor_long);
            }

            else if (interpretacion.compare("word") == 0)
            {
                rc = modbus_read_registers(ctx_mbtcp, registro, cantidad1, datos);

                if ((bit1 >= 0) && (bit1 <= 15)) {
                    valor = obtenervalorbit(datos[0], bit1);
                }
                else {
                    valor = datos[0];
                }

                sprintf(buffer, "%i", valor);
            }

            // ========================================================
            // NUEVA INTERPRETACION: WORD CON SIGNO (INT16)
            // ========================================================

            else if (interpretacion.compare("word_signed") == 0)
            {
                rc = modbus_read_registers(ctx_mbtcp, registro, cantidad1, datos);

                int16_t valor_signed = 0;

                if (rc != -1) {
                    if ((bit1 >= 0) && (bit1 <= 15)) {
                        valor_signed = obtenervalorbit(datos[0], bit1);
                    }
                    else {
                        valor_signed = static_cast<int16_t>(datos[0]);
                    }
                }

                snprintf(buffer, sizeof(buffer), "%d", static_cast<int>(valor_signed));
            }

            else
            {
                cout << "En \"" << nombre << "\" tipo no def!!: " << interpretacion << endl;
            }
        }

        // ============================================================
        // INPUT REGISTERS (rr)
        // ============================================================

        if (funcion.compare("rr") == 0) {

            if (interpretacion.compare("float32abcd") == 0) {
                rc = modbus_read_input_registers(ctx_mbtcp, registro, 2, datos);
                valor_float = cast_get_float_abcd(datos);
                sprintf(buffer, "%f", valor_float);
            }

            else if (interpretacion.compare("float32badc") == 0)
            {
                rc = modbus_read_input_registers(ctx_mbtcp, registro, 2, datos);
                valor_float = cast_get_float_badc(datos);
                sprintf(buffer, "%f", valor_float);
            }

            else if (interpretacion.compare("float32cdab") == 0)
            {
                rc = modbus_read_input_registers(ctx_mbtcp, registro, 2, datos);
                valor_float = cast_get_float_cdab(datos);
                sprintf(buffer, "%f", valor_float);
            }

            else if (interpretacion.compare("float32dcba") == 0)
            {
                rc = modbus_read_input_registers(ctx_mbtcp, registro, 2, datos);
                valor_float = cast_get_float_dcba(datos);
                sprintf(buffer, "%f", valor_float);
            }

            else if (interpretacion.compare("longle") == 0)
            {
                rc = modbus_read_input_registers(ctx_mbtcp, registro, 2, datos);
                valor_long = datos[0] + (datos[1] << 16);
                sprintf(buffer, "%i", valor_long);
            }

            else if (interpretacion.compare("longbe") == 0)
            {
                rc = modbus_read_input_registers(ctx_mbtcp, registro, 2, datos);
                valor_long = datos[1] + (datos[0] << 16);
                sprintf(buffer, "%i", valor_long);
            }

            else if (interpretacion.compare("word") == 0)
            {
                rc = modbus_read_input_registers(ctx_mbtcp, registro, cantidad1, datos);

                if ((bit1 >= 0) && (bit1 <= 15)) {
                    valor = obtenervalorbit(datos[0], bit1);
                }
                else {
                    valor = datos[0];
                }

                sprintf(buffer, "%i", valor);
            }

            // ========================================================
            // NUEVA INTERPRETACION: WORD CON SIGNO (INT16)
            // ========================================================

            else if (interpretacion.compare("word_signed") == 0)
            {
                rc = modbus_read_input_registers(ctx_mbtcp, registro, cantidad1, datos);

                int16_t valor_signed = 0;

                if (rc != -1) {
                    if ((bit1 >= 0) && (bit1 <= 15)) {
                        valor_signed = obtenervalorbit(datos[0], bit1);
                    }
                    else {
                        valor_signed = static_cast<int16_t>(datos[0]);
                    }
                }

                snprintf(buffer, sizeof(buffer), "%d", static_cast<int>(valor_signed));
            }

            else
            {
                cout << "En \"" << nombre << "\" tipo no def!!: " << interpretacion << endl;
            }
        }

        // ============================================================
        // COILS (cs)
        // ============================================================

        if (funcion.compare("cs") == 0) {
            rc = modbus_read_bits(ctx_mbtcp, registro, 1, datos8b);
            sprintf(buffer, "%i", datos8b[0]);
            memcpy(datos, datos8b, 4);
        }

        // ============================================================
        // DISCRETE INPUTS (ir)
        // ============================================================

        if (funcion.compare("ir") == 0) {
            rc = modbus_read_input_bits(ctx_mbtcp, registro, 1, datos8b);
            sprintf(buffer, "%i", datos8b[0]);
            memcpy(datos, datos8b, 4);
        }

    } // fin del while

    // ================================================================
    // RESULTADO DE LA LECTURA
    // ================================================================

    if (rc != -1) {

        setvalorstring(cast_arrchar_to_string(buffer));
        resultado_leer = true;
        *pcontador_errores = 0;

        ssdatos << "dato[0]: " << datos[0] << " dato[1]: " << datos[1];
        imprimir_mensaje(ssdatos.str(), negro);

        imprimir_mensaje("valor float: " + cast_float_to_string(atof(buffer)), negro);
        imprimir_mensaje("valor float escalado: " + cast_float_to_string(clsvalorfloat), negro);

        memcpy(&static_clsdatosleidos_mbtcp[0], &datos[0], 20);

        for (int offset1 = 0; offset1 < 20; offset1++) {
            if (offset1 > cantidad1) {
                static_clsregistros_mbtcp[offset1] = -1;
            }
            else {
                static_clsregistros_mbtcp[offset1] = registro + offset1;
            }
        }

        //leido del tag

        //clsvalor = atoi(buffer);
        //clsvalorfloat = atof(buffer) * escalamiento;
        //clsvalorstr = cast_float_to_string(clsvalorfloat);
    }

    else {
        resultado_leer = false;
        incrementarerrores();
        descripcionerror = rastro + " - error modbus";
        logger.registrarError("WARN: Modbus TCP read fail. "+rastro);
        imprimir_mensaje(descripcionerror, rojo);
    }

    if (conectadocondispositivo) {
        modbus_close(ctx_mbtcp);
        modbus_free(ctx_mbtcp);
    };

    resultadoleer_rc = rc;
};


 // -------------------------------------------------------------------------
/* void cls_equipos::leer_mbserial(void) {
	modbus_t* ctx_mbserial = NULL;
	bool conectadocondispositivo = true;
	string mensajedeerror = "";
	int intento = 0;
	int intentosmax = 3;
	uint16_t datos[20]; //reserva suficiente espacio para 10 registros long o 20 reg word
	uint8_t datos8b[4];
	float valor_float;
	uint32_t valor_long;
	uint16_t valor;
	char buffer[4];
	stringstream ssdatos;
	int rc = -1;

	static_clsnombre_mbserial = nombre;
	ctx_mbserial = modbus_new_rtu(interface1.c_str(), velocidad, paridad.c_str()[0], numerodebits, bitsparada);
	rastro = nombre + ";" + interface1 + ";" + "idslave=" + cast_int_to_string(direccion_esclavo) + ";" + tag + ";" + "reg=" + cast_int_to_string(registro);
	
	if (ctx_mbserial == NULL) {
			fprintf(stderr, "=falla en iniciar libmodbus\n");//Loggin
			logger.registrarError("DEBUG: libmodbus fail start in: leer_mbserial() : "+ std::string(__FILE__));
			conectadocondispositivo = false;
			resultado_leer = false;
			rc = -1;
	}

	if (direccion_esclavo == 0) direccion_esclavo = 1;
	modbus_set_slave(ctx_mbserial, direccion_esclavo);
	if (modbus_connect(ctx_mbserial) == -1) {
			string smsmodbus = string(modbus_strerror(errno));
			mensajedeerror = "fallo en modbus errno= " + smsmodbus + " ";//Loggin
			mensajedeerror += rastro;
			imprimir_mensaje(mensajedeerror, rojo);
			logger.registrarError("WARN: Modbus serial fail."+smsmodbus+" "+rastro);
			modbus_free(ctx_mbserial);
			conectadocondispositivo = false;
			resultado_leer = false;
			rc = -1;
		}
		// lee los holding registers
		rc = -1;
		intento = 0;
		if (cantidad1 > 20) cantidad1 = 20;
		//modbus_set_response_timeout(ctx_mbserial, 0, 200000);

		while ((intento < intentosmax) && (rc == -1) && (conectadocondispositivo == true)) { //while 1
			  //cout << "la funcion es: " << funcion << endl;
			  //cout << "la interpretacion es: " << interpretacion << endl;
			intento++;
			//cout << "leer:ln115" << endl;	
			memset(datos, 0, 20 * sizeof(uint16_t));
			memset(datos8b, 0, 4 * sizeof(uint8_t));
			memset(buffer, 0, 4 * sizeof(char));
			valor_float = 0.0;
			valor_long = 0;
			valor = 0;
			if (intento > 1) { esperar(0.1); };

			if (funcion.compare("hr") == 0) {

				if (interpretacion.compare("float32abcd") == 0) {
					rc = modbus_read_registers(ctx_mbserial, registro, 2, datos);
					valor_float = cast_get_float_abcd(datos);
					sprintf(buffer, "%f", valor_float);
					//arr_valores[i] = string(buffer);
					//cout << "1*";
				}
				else if (interpretacion.compare("float32badc") == 0)
				{
					rc = modbus_read_registers(ctx_mbserial, registro, 2, datos);
					valor_float = cast_get_float_badc(datos);
					sprintf(buffer, "%f", valor_float);
					//arr_valores[i] = string(buffer);
					//cout << "2*";				
				}
				else if (interpretacion.compare("float32cdab") == 0)
				{
					rc = modbus_read_registers(ctx_mbserial, registro, 2, datos);
					valor_float = cast_get_float_cdab(datos);
					sprintf(buffer, "%f", valor_float);
					//arr_valores[i] = string(buffer);
					//cout << "2*";				
				}
				else if (interpretacion.compare("float32dcba") == 0)
				{

					rc = modbus_read_registers(ctx_mbserial, registro, 2, datos);
					//cout << "dato_0:" << datos[0] << "   " << "dato_1:" << datos[1] << endl;
					valor_float = cast_get_float_dcba(datos);
					sprintf(buffer, "%f", valor_float);
					//cout << (rc != -1)? string(buffer) : "0.0";
					//imprimir_mensaje(cast_float_to_string(valor_float), negro);

				}
				else if (interpretacion.compare("float32fp") == 0) // solo para los PM factor de potencia
				{
					rc = modbus_read_registers(ctx_mbserial, registro, 2, datos);
					valor_float = cast_get_float_dcba(datos);
					valor_float = convert_float_fp(valor_float);
					sprintf(buffer, "%f", valor_float);
					//arr_valores[i] = (rc != -1)? string(buffer) : "0.0";
					//cout << "2*";				
				}
				else if (interpretacion.compare("longle") == 0)
				{
					rc = modbus_read_registers(ctx_mbserial, registro, 2, datos);
					valor_long = datos[0] + (datos[1] << 16);
					sprintf(buffer, "%f", valor_long);
					//arr_valores[i] = string(buffer);
					//cout << "3*";								
				}

				else if (interpretacion.compare("longle64") == 0)
				{
					rc = modbus_read_registers(ctx_mbserial, registro, 4, datos);
					valor_long = datos[0] + (datos[1] << 16) + (datos[2] << 32) + (datos[3] << 48);
					sprintf(buffer, "%i", valor_long);
					//arr_valores[i] = string(buffer);
					//cout << "3*";								
				}

				else if (interpretacion.compare("longbe") == 0)
				{
					rc = modbus_read_registers(ctx_mbserial, registro, 2, datos);
					valor_long = datos[1] + (datos[0] << 16);
					//cout << "valor long be:" << valor_long << endl;
					sprintf(buffer, "%i", valor_long);
					//cout << "valor buffer:" << buffer << endl;
					//arr_valores[i] = string(buffer);
					//cout << "4*";								
				}

				else if (interpretacion.compare("word") == 0)
				{
					rc = modbus_read_registers(ctx_mbserial, registro, cantidad1, datos);

					if ((bit1 >= 0) && (bit1 <= 15)) {
						valor = obtenervalorbit(datos[0], bit1);
					}
					else {
						valor = datos[0];
					}
					sprintf(buffer, "%i", valor);
					//arr_valores[i] = (rc != -1)? string(buffer) : "0";
					//cout << "interpretado como word registro:" << registro << "valor: " << valor << endl;								
				}
				//
				else
				{
					cout << "En \"" << nombre << "\" tipo no def!!: " << interpretacion << endl;


				}
			}

			if (funcion.compare("rr") == 0) {
				if (interpretacion.compare("float32abcd") == 0) {
					rc = modbus_read_input_registers(ctx_mbserial, registro, 2, datos);
					valor_float = cast_get_float_abcd(datos);
					sprintf(buffer, "%f", valor_float);
					//arr_valores[i] = string(buffer);				
				}
				else if (interpretacion.compare("float32badc") == 0)
				{
					rc = modbus_read_input_registers(ctx_mbserial, registro, 2, datos);
					valor_float = cast_get_float_badc(datos);
					sprintf(buffer, "%f", valor_float);
					//arr_valores[i] = string(buffer);				
				}
				//float32cdab
				else if (interpretacion.compare("float32cdab") == 0)
				{
					rc = modbus_read_input_registers(ctx_mbserial, registro, 2, datos);
					valor_float = cast_get_float_cdab(datos);
					sprintf(buffer, "%f", valor_float);
					//arr_valores[i] = string(buffer);				
				}
				//float32dcba
				else if (interpretacion.compare("float32dcba") == 0)
				{
					rc = modbus_read_input_registers(ctx_mbserial, registro, 2, datos);
					valor_float = cast_get_float_dcba(datos);
					sprintf(buffer, "%f", valor_float);
					//arr_valores[i] = string(buffer);				
				}
				else if (interpretacion.compare("longle") == 0)
				{
					rc = modbus_read_input_registers(ctx_mbserial, registro, 2, datos);
					valor_long = datos[0] + (datos[1] << 16);
					sprintf(buffer, "%i", valor_long);
					//arr_valores[i] = string(buffer);				
				}

				else if (interpretacion.compare("longbe") == 0)
				{
					rc = modbus_read_input_registers(ctx_mbserial, registro, 2, datos);
					valor_long = datos[1] + (datos[0] << 16);
					//cout << "valor long be:" << valor_long << endl;

					sprintf(buffer, "%i", valor_long);
					//arr_valores[i] = string(buffer);
					//cout << "valor buffer:" << buffer << endl;
				}
				else if (interpretacion.compare("word") == 0)
				{
					rc = modbus_read_input_registers(ctx_mbserial, registro, cantidad1, datos);

					if ((bit1 >= 0) && (bit1 <= 15)) {
						valor = obtenervalorbit(datos[0], bit1);
					}
					else {
						valor = datos[0];
					}
					sprintf(buffer, "%i", valor);
					//arr_valores[i] = string(buffer);				
				}
				else
				{
					cout << "En \"" << nombre << "\" tipo no def!!: " << interpretacion << endl;


				}
			} // fin del if del read register

			if (funcion.compare("cs") == 0) {
				rc = modbus_read_bits(ctx_mbserial, registro, 1, datos8b);
				sprintf(buffer, "%i", datos8b[0]);
				memcpy(datos, datos8b, 4);
			}


			if (funcion.compare("ir") == 0) {
				rc = modbus_read_input_bits(ctx_mbserial, registro, 1, datos8b);
				sprintf(buffer, "%i", datos8b[0]);
				memcpy(datos, datos8b, 4);

			}

		} // fin del while 1

		if (rc != -1) {


			memcpy(&static_clsdatosleidos_mbserial[0], &datos[0], 20);
			for (int offset1 = 0; offset1 < 20; offset1++) {
					if (offset1 > cantidad1) {
						static_clsregistros_mbserial[offset1] = -1;
					}
					else {
						static_clsregistros_mbserial[offset1] = registro + offset1;
					}

			}


			ssdatos << "dato[0]: " << datos[0] << " dato[1]: " << datos[1];
			imprimir_mensaje(ssdatos.str(), negro);

			setvalorstring(cast_arrchar_to_string(buffer)); // esta funcion actualiza el valor
															//leido del tag
			imprimir_mensaje("valor float: " + cast_float_to_string(atof(buffer)), negro);
			imprimir_mensaje("valor float escalado: " + cast_float_to_string(clsvalorfloat), negro);
			
			resultado_leer = true;
			*pcontador_errores = 0;

		}
		else {
			incrementarerrores();
			descripcionerror = rastro + " - error modbus";//Loggin
			logger.registrarError("WARN: Modbus serial read fail. "+rastro);
			imprimir_mensaje(descripcionerror, rojo);
		}

		if (conectadocondispositivo) {
			modbus_close(ctx_mbserial);
			modbus_free(ctx_mbserial);
		};

		resultadoleer_rc = rc;
};
 */
//-------------
//leer_mbserial() Variables tipo Word con signo


void cls_equipos::leer_mbserial(void) {
    modbus_t* ctx_mbserial = NULL;
    bool conectadocondispositivo = true;
    string mensajedeerror = "";
    int intento = 0;
    int intentosmax = 3;
    uint16_t datos[20]; //reserva suficiente espacio para 10 registros long o 20 reg word
    uint8_t datos8b[4];
    float valor_float;
    uint32_t valor_long;
    uint16_t valor;
    char buffer[32];
    stringstream ssdatos;
    int rc = -1;

    static_clsnombre_mbserial = nombre;
    ctx_mbserial = modbus_new_rtu(interface1.c_str(), velocidad, paridad.c_str()[0], numerodebits, bitsparada);
    rastro = nombre + ";" + interface1 + ";" + "idslave=" + cast_int_to_string(direccion_esclavo) + ";" + tag + ";" + "reg=" + cast_int_to_string(registro);

    if (ctx_mbserial == NULL) {
        fprintf(stderr, "=falla en iniciar libmodbus\n");//Loggin
        logger.registrarError("DEBUG: libmodbus fail start in: leer_mbserial() : " + std::string(__FILE__));
        conectadocondispositivo = false;
        resultado_leer = false;
        rc = -1;
    }

    if (direccion_esclavo == 0) direccion_esclavo = 1;
    modbus_set_slave(ctx_mbserial, direccion_esclavo);

    if (modbus_connect(ctx_mbserial) == -1) {
        string smsmodbus = string(modbus_strerror(errno));
        mensajedeerror = "fallo en modbus errno= " + smsmodbus + " ";//Loggin
        mensajedeerror += rastro;
        imprimir_mensaje(mensajedeerror, rojo);
        logger.registrarError("WARN: Modbus serial fail." + smsmodbus + " " + rastro);
        modbus_free(ctx_mbserial);
        conectadocondispositivo = false;
        resultado_leer = false;
        rc = -1;
    }

    // lee los holding registers
    rc = -1;
    intento = 0;
    if (cantidad1 > 20) cantidad1 = 20;
    //modbus_set_response_timeout(ctx_mbserial, 0, 200000);

    while ((intento < intentosmax) && (rc == -1) && (conectadocondispositivo == true)) {

        intento++;

        memset(datos, 0, 20 * sizeof(uint16_t));
        memset(datos8b, 0, 4 * sizeof(uint8_t));
        memset(buffer, 0, sizeof(buffer));

        valor_float = 0.0;
        valor_long = 0;
        valor = 0;

        if (intento > 1) { esperar(0.1); };

        // ============================================================
        // HOLDING REGISTERS (hr)
        // ============================================================

        if (funcion.compare("hr") == 0) {

            if (interpretacion.compare("float32abcd") == 0) {
                rc = modbus_read_registers(ctx_mbserial, registro, 2, datos);
                valor_float = cast_get_float_abcd(datos);
                sprintf(buffer, "%f", valor_float);
            }

            else if (interpretacion.compare("float32badc") == 0)
            {
                rc = modbus_read_registers(ctx_mbserial, registro, 2, datos);
                valor_float = cast_get_float_badc(datos);
                sprintf(buffer, "%f", valor_float);
            }

            else if (interpretacion.compare("float32cdab") == 0)
            {
                rc = modbus_read_registers(ctx_mbserial, registro, 2, datos);
                valor_float = cast_get_float_cdab(datos);
                sprintf(buffer, "%f", valor_float);
            }

            else if (interpretacion.compare("float32dcba") == 0)
            {
                rc = modbus_read_registers(ctx_mbserial, registro, 2, datos);
                valor_float = cast_get_float_dcba(datos);
                sprintf(buffer, "%f", valor_float);
            }

            else if (interpretacion.compare("float32fp") == 0)
            {
                // solo para los PM factor de potencia
                rc = modbus_read_registers(ctx_mbserial, registro, 2, datos);
                valor_float = cast_get_float_dcba(datos);
                valor_float = convert_float_fp(valor_float);
                sprintf(buffer, "%f", valor_float);
            }

            else if (interpretacion.compare("longle") == 0)
            {
                rc = modbus_read_registers(ctx_mbserial, registro, 2, datos);
                valor_long = datos[0] + (datos[1] << 16);
                sprintf(buffer, "%f", valor_long);
            }

            else if (interpretacion.compare("longle64") == 0)
            {
                rc = modbus_read_registers(ctx_mbserial, registro, 4, datos);
                valor_long = datos[0] + (datos[1] << 16) + (datos[2] << 32) + (datos[3] << 48);
                sprintf(buffer, "%i", valor_long);
            }

            else if (interpretacion.compare("longbe") == 0)
            {
                rc = modbus_read_registers(ctx_mbserial, registro, 2, datos);
                valor_long = datos[1] + (datos[0] << 16);
                sprintf(buffer, "%i", valor_long);
            }

            else if (interpretacion.compare("word") == 0)
            {
                rc = modbus_read_registers(ctx_mbserial, registro, cantidad1, datos);

                if ((bit1 >= 0) && (bit1 <= 15)) {
                    valor = obtenervalorbit(datos[0], bit1);
                }
                else {
                    valor = datos[0];
                }

                sprintf(buffer, "%i", valor);
            }

            // ========================================================
            // NUEVA INTERPRETACION: WORD CON SIGNO (INT16)
            // ========================================================

            else if (interpretacion.compare("word_signed") == 0)
            {
                rc = modbus_read_registers(ctx_mbserial, registro, cantidad1, datos);

                int16_t valor_signed = 0;

                if (rc != -1) {
                    if ((bit1 >= 0) && (bit1 <= 15)) {
                        valor_signed = obtenervalorbit(datos[0], bit1);
                    }
                    else {
                        valor_signed = static_cast<int16_t>(datos[0]);
                    }
                }

                snprintf(buffer, sizeof(buffer), "%d", static_cast<int>(valor_signed));
            }

            else
            {
                cout << "En \"" << nombre << "\" tipo no def!!: " << interpretacion << endl;
            }
        }

        // ============================================================
        // INPUT REGISTERS (rr)
        // ============================================================

        if (funcion.compare("rr") == 0) {

            if (interpretacion.compare("float32abcd") == 0) {
                rc = modbus_read_input_registers(ctx_mbserial, registro, 2, datos);
                valor_float = cast_get_float_abcd(datos);
                sprintf(buffer, "%f", valor_float);
            }

            else if (interpretacion.compare("float32badc") == 0)
            {
                rc = modbus_read_input_registers(ctx_mbserial, registro, 2, datos);
                valor_float = cast_get_float_badc(datos);
                sprintf(buffer, "%f", valor_float);
            }

            else if (interpretacion.compare("float32cdab") == 0)
            {
                rc = modbus_read_input_registers(ctx_mbserial, registro, 2, datos);
                valor_float = cast_get_float_cdab(datos);
                sprintf(buffer, "%f", valor_float);
            }

            else if (interpretacion.compare("float32dcba") == 0)
            {
                rc = modbus_read_input_registers(ctx_mbserial, registro, 2, datos);
                valor_float = cast_get_float_dcba(datos);
                sprintf(buffer, "%f", valor_float);
            }

            else if (interpretacion.compare("longle") == 0)
            {
                rc = modbus_read_input_registers(ctx_mbserial, registro, 2, datos);
                valor_long = datos[0] + (datos[1] << 16);
                sprintf(buffer, "%i", valor_long);
            }

            else if (interpretacion.compare("longbe") == 0)
            {
                rc = modbus_read_input_registers(ctx_mbserial, registro, 2, datos);
                valor_long = datos[1] + (datos[0] << 16);
                sprintf(buffer, "%i", valor_long);
            }

            else if (interpretacion.compare("word") == 0)
            {
                rc = modbus_read_input_registers(ctx_mbserial, registro, cantidad1, datos);

                if ((bit1 >= 0) && (bit1 <= 15)) {
                    valor = obtenervalorbit(datos[0], bit1);
                }
                else {
                    valor = datos[0];
                }

                sprintf(buffer, "%i", valor);
            }

            // ========================================================
            // NUEVA INTERPRETACION: WORD CON SIGNO (INT16)
            // ========================================================

            else if (interpretacion.compare("word_signed") == 0)
            {
                rc = modbus_read_input_registers(ctx_mbserial, registro, cantidad1, datos);

                int16_t valor_signed = 0;

                if (rc != -1) {
                    if ((bit1 >= 0) && (bit1 <= 15)) {
                        valor_signed = obtenervalorbit(datos[0], bit1);
                    }
                    else {
                        valor_signed = static_cast<int16_t>(datos[0]);
                    }
                }

                snprintf(buffer, sizeof(buffer), "%d", static_cast<int>(valor_signed));
            }

            else
            {
                cout << "En \"" << nombre << "\" tipo no def!!: " << interpretacion << endl;
            }
        }

        // ============================================================
        // COILS (cs)
        // ============================================================

        if (funcion.compare("cs") == 0) {
            rc = modbus_read_bits(ctx_mbserial, registro, 1, datos8b);
            sprintf(buffer, "%i", datos8b[0]);
            memcpy(datos, datos8b, 4);
        }

        // ============================================================
        // DISCRETE INPUTS (ir)
        // ============================================================

        if (funcion.compare("ir") == 0) {
            rc = modbus_read_input_bits(ctx_mbserial, registro, 1, datos8b);
            sprintf(buffer, "%i", datos8b[0]);
            memcpy(datos, datos8b, 4);
        }

    } // fin del while

    // ================================================================
    // RESULTADO DE LA LECTURA
    // ================================================================

    if (rc != -1) {

        memcpy(&static_clsdatosleidos_mbserial[0], &datos[0], 20);

        for (int offset1 = 0; offset1 < 20; offset1++) {
            if (offset1 > cantidad1) {
                static_clsregistros_mbserial[offset1] = -1;
            }
            else {
                static_clsregistros_mbserial[offset1] = registro + offset1;
            }
        }

        ssdatos << "dato[0]: " << datos[0] << " dato[1]: " << datos[1];
        imprimir_mensaje(ssdatos.str(), negro);

        setvalorstring(cast_arrchar_to_string(buffer));

        imprimir_mensaje("valor float: " + cast_float_to_string(atof(buffer)), negro);
        imprimir_mensaje("valor float escalado: " + cast_float_to_string(clsvalorfloat), negro);

        resultado_leer = true;
        *pcontador_errores = 0;
    }

    else {
        incrementarerrores();
        descripcionerror = rastro + " - error modbus";//Loggin
        logger.registrarError("WARN: Modbus serial read fail. " + rastro);
        imprimir_mensaje(descripcionerror, rojo);
    }

    if (conectadocondispositivo) {
        modbus_close(ctx_mbserial);
        modbus_free(ctx_mbserial);
    };

    resultadoleer_rc = rc;
};


//-------------------------------------
void cls_equipos::leer_htmls9(void) {
	int rc = -1;
	CURL* curl;
	FILE* fp;
	FILE* fp2;
	CURLcode result;
	char* urlminador;
	char* archivo;
	char* usuariopass;
	char caracter;
	string buffer1;
	size_t ubicacionanterior;
	size_t found1;
	size_t found2;
	size_t found3;
	string valorstr = "";
	size_t valorstr_len;
	string delimitadorinicio;
	string delimitadorfinal;
	int i = 0;
	bool bandera1 = false;
	bool bandera2 = false;
	vector<string> vector_valores1;
	resultado_leer = false;
	int html_cont_temperaturas = 0;
	stringstream ssdatos;
	bool conectadocondispositivo = true;
	string mensajedeerror = "";

	urlminador = (char*)calloc(256, sizeof(char));
	archivo = (char*)calloc(32, sizeof(char));
	usuariopass = (char*)calloc(32, sizeof(char));
	//strcpy(urlminador, url_miner); //"http://192.168.0.105/cgi-bin/minerStatus.cgi"
	sprintf(urlminador, url_miner.c_str(), direccion_ip.c_str());
	sprintf(usuariopass, "%s:%s", usuario.c_str(), password.c_str());
	//strcpy(archivo, "log2.txt");
	sprintf(archivo, "log%s_%u.txt", direccion_ip.c_str(), huella(url_miner));
	string archivo_nuevo = string(archivo);
	time_t archivo_nuevo_tiempo = time(NULL);
	string valoretiquetastr;
	rastro = nombre + ";" + direccion_ip;
	if ((archivo_nuevo.compare(clsarchivo) != 0) || (difftime(archivo_nuevo_tiempo, clsarchivo_tiempo) > 20.0)) {
		imprimir_mensaje("lectura html: " + rastro, negro);
		fp = std::fopen(archivo, "wb");
		if (fp == NULL) {
			incrementarerrores();
			return;
		}
		curl = curl_easy_init();

		if (curl) { // inicio del if 1
			curl_easy_setopt(curl, CURLOPT_URL, urlminador);
			curl_easy_setopt(curl, CURLOPT_WRITEDATA, fp);
			curl_easy_setopt(curl, CURLOPT_FAILONERROR, 1L);
			curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, 10L);
			curl_easy_setopt(curl, CURLOPT_TIMEOUT, 20L);
			curl_easy_setopt(curl, CURLOPT_USERPWD, usuariopass);
			curl_easy_setopt(curl, CURLOPT_HTTPAUTH, CURLAUTH_ANY);
			result = curl_easy_perform(curl);

			if (result == CURLE_OK) { //inicio del if 2
				bandera2 = true;
				cls_equipos::clsarchivo = string(archivo);
				cls_equipos::clsarchivo_tiempo = time(NULL);
				//printf("leer minador | %s - archivo cgi ok! \n", direccion_ip.c_str());
			}

			else
			{ // 3
				printf("leer minador | %s - error %s \n", direccion_ip.c_str(), curl_easy_strerror(result));
				//clsvalor = 0;
				//clsvalorfloat = 0.0;
				//clsvalorstr = "no responde";
				logger.registrarError("ERROR: " + std::string(curl_easy_strerror(result)) + " .In miner: " + direccion_ip);
				incrementarerrores();
				descripcionerror = rastro + " - no hay comunicacion - web server no responde! - ";
				imprimir_mensaje(descripcionerror, rojo);
				logger.registrarError("ERROR: Communication error.The web server is not responding. " +rastro);
				rc = -2;
				//return -2;

			}; // fin 3
		};		// fin del if 1
		
		if (fp != NULL) std::fclose(fp);

		curl_easy_cleanup(curl);

	}
	else {
		imprimir_mensaje("se usara el mismo archivo: " + rastro, negro);
		bandera2 = true;
	}

	if (bandera2) {

		// abre el archivo como solo lectura
		fp2 = std::fopen(archivo, "r");
		if (fp2 == NULL)
		{
			rc = -3;
			incrementarerrores();
			descripcionerror = rastro + " - comunicacion buena con dispositivo pero el archivo resultado no se puede leer!";
			logger.registrarError("WARN: Result file cannot be read. "+rastro);
			imprimir_mensaje(descripcionerror, rojo);
			//return -3;		
			;
		}
		else {
			while (!feof(fp2))
			{
				caracter = fgetc(fp2);
				i += 1;
				buffer1 = buffer1 + caracter;
				if (i > 62000) { break; };
			};

			ubicacionanterior = 0;
			delimitadorinicio = html_id;
			delimitadorfinal = html_final;
			//-1 no consiguio la etiqueta y 1 si la consiguio
			rc = leeretiqueta(buffer1, delimitadorinicio, delimitadorfinal, valoretiquetastr, ubicacionanterior, html_posicion);
			imprimir_mensaje("ini: " + delimitadorinicio + " | " + valoretiquetastr + " | fin: " + delimitadorfinal, negro);
			vector_valores1 = split1(valoretiquetastr, html_separador[0]);
			imprimir_mensaje("dimension: " + cast_int_to_string(vector_valores1.size()), negro);
			if (rc > 0) {
				if (vector_valores1.size() > html_indice) {

					setvalorstring(vector_valores1[html_indice].substr(0, 20)); // esta funcion actualiza el valor
												//leido del tag

					imprimir_mensaje("valor: " + clsvalorstr, negro);
					*pcontador_errores = 0;
				}
			}
			else {
				incrementarerrores();
				descripcionerror = rastro + " - comunicacion buena dispositivo pero tag no encontrado!";
				logger.registrarError("WARN: Tag not found. "+rastro);
				imprimir_mensaje(descripcionerror, rojo);
				//cout << buffer1 << endl;
			}

		}; // fin de if del archivo abrir archivo fp2 para lectura

		if (fp2 != NULL) std::fclose(fp2);

	}; // fin del if de bandera2 que equivale a CURLE_OK

	resultadoleer_rc = rc;

};

void cls_equipos::leer_rs232(void) {

	int largoventana = tamanoventana;
	string tmpstr = "";
	string token = "";
	size_t pos1, pos2;
	vector<stdelim> vector_stdelim;
	int longitudtoken = 0;
	resultadoleer_rc = -1;
	stdelim estructura1;
	estructura1.tag = tag;
	estructura1.delim1 = html_id;
	estructura1.delim2 = html_final;
	//cout << buf << "\t fin bufer";
	vector_stdelim.push_back(estructura1);
	token.clear();
	rastro = nombre + ";" + interface1 + ";" + tag;
	if (*p_n_buf > 0) // if 1
		{
			buf[*p_n_buf] = 0;
			for (int i = 0; i < *p_n_buf; i++) { // for1
				tmpstr.append((char*)&buf[i], 1);
				if (tmpstr.size() > (largoventana + 1)) {
					tmpstr.erase(0, 1);
				};

				for (stdelim st : vector_stdelim) { //for 2
					pos1 = string::npos;
					pos2 = string::npos;
					pos1 = tmpstr.find(st.delim1);
					if (pos1 != string::npos) { //if3
						pos2 = tmpstr.find(st.delim2, pos1 + st.delim1.length());
						if (pos2 != string::npos) { //if4
							longitudtoken = pos2 - pos1 - st.delim1.length();
							token = tmpstr.substr(pos1 + st.delim1.length(), longitudtoken);
							st.valor = token;
							setvalorstring(limpiarcadena(token));
							resultadoleer_rc = 1;
								tmpstr.erase(pos1, st.delim1.length() + longitudtoken);
							} //if4
					} //if3
				}; // for2
			} // for 1

		} //if1

};



/* Get a float from 4 bytes (Modbus) in inversed format (DCBA) */
float cls_equipos::convert_float_fp(float regVal)
{
	float PF_Val;

	if (regVal > 1)
	{
		PF_Val = 2 - regVal;
		//PF is leading
	}
	else if (regVal < -1)
	{
		PF_Val = -2 - regVal;
		//PF is leading
	}
	else if (regVal == 1.0)
	{
		PF_Val = regVal;
		//PF is at unity
	}
	else
	{
		PF_Val = regVal;
		//PF is lagging
	}
	return PF_Val;
}


void cls_equipos::esperar(double segundos) {
    if (!std::isfinite(segundos) || segundos <= 0.0) {
        return;
    }

    std::this_thread::sleep_for(
        std::chrono::duration<double>(segundos)
    );
}


int cls_equipos::resetear_s9(void) {
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
	fp = std::fopen(archivo, "wb");
	curl = curl_easy_init();
	if (curl) { // inicio del if 1
		curl_easy_setopt(curl, CURLOPT_URL, urlminador);
		curl_easy_setopt(curl, CURLOPT_WRITEDATA, fp);
		curl_easy_setopt(curl, CURLOPT_FAILONERROR, 1L);
		curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, 10L);
		curl_easy_setopt(curl, CURLOPT_TIMEOUT, 20L);
		curl_easy_setopt(curl, CURLOPT_USERPWD, usuariopass);
		curl_easy_setopt(curl, CURLOPT_HTTPAUTH, CURLAUTH_ANY);

		//--connect-timeout <seconds>
		result = curl_easy_perform(curl);

		if (result == CURLE_OK) { //inicio del if 2
			printf("%s - reset del ant miner s9 ok! \n", direccion_ip.c_str());
			rc = 1;
		}

		else
		{ // 3
			printf("%s - reset del ant miner - error %s \n", direccion_ip.c_str(), curl_easy_strerror(result));
			return -1;
		}; // fin 3
	};		// fin del if 1
	if (fp != NULL) std::fclose(fp);

	curl_easy_cleanup(curl);

	return rc;
}


void cls_equipos::setvalorfloat(float nuevovalor) {

	oldclsvalorfloat = clsvalorfloat;
	oldclsvalor = clsvalor;
	oldclsvalorstr = clsvalorstr;

	clsvalorfloat =  nuevovalor*escalamiento;
	clsvalor = (int) clsvalorfloat;
	clsvalorstr = cast_float_to_string(clsvalorfloat);

	if (oldclsvalorfloat == clsvalorfloat) {
		fuecambiado = false;
	}
	else {
		fuecambiado = true;
	}

}


void cls_equipos::setvalorstring(string nuevovalorstr) {
	
	if (nuevovalorstr.length() == 0) {
		return;
	}

	oldclsvalorstr = clsvalorstr;
	oldclsvalorfloat = clsvalorfloat;
	oldclsvalor = clsvalor;
	
	clsvalorfloat = cast_string_to_float(limpiarcadena(nuevovalorstr)) * escalamiento;
	clsvalor = (int)clsvalorfloat;
	if (clsvalorfloat == 0.0) {
		clsvalorstr = limpiarcadena(nuevovalorstr);
	}
	else {
		clsvalorstr = cast_float_to_string(clsvalorfloat);
	}

	
	if (oldclsvalorstr.compare(clsvalorstr) == 0) {
		fuecambiado = false; 
		

	}
	else {
		//cout << "fue cambiado! " << "viejo:\t" << oldclsvalorstr << "actual:\t" << clsvalorstr <<"nuevo:\t" << nuevovalorstr << endl;
		fuecambiado = true;
		fueescritodb = false;
	}

	
	return;
}