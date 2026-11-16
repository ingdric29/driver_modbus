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
#include "include/modbus_decode.h"
#include <limits>
#include "include/modbus_transport.h"
#include "include/register_batch.h"
#include "include/runtime_control.h"
#include "include/driver_metrics.h"
#include "include/monitor_state.h"
namespace {
struct MonitorRead {
    cls_equipos& tag;
    monitor::Clock::time_point start = monitor::Clock::now();
    explicit MonitorRead(cls_equipos& t) : tag(t) {}
    ~MonitorRead() {
        auto us = std::chrono::duration_cast<std::chrono::microseconds>(monitor::Clock::now() - start).count();
        monitor::State::instance().publish(tag.monitor_key, tag.resultadoleer_rc, tag.clsvalorstr,
            tag.latest_sample.acquired, us, register_acquired_steady() == monitor::Clock::time_point{}
                ? monitor::Clock::now() : register_acquired_steady());
    }
};
}
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
int cls_equipos::static_clsregistros_mbserial[20] = {};
int cls_equipos::static_clsdatosleidos_mbserial[20] = {};

string cls_equipos::static_clsnombre_mbtcp = "";
time_t cls_equipos::static_clsultimalectura_mbtcp = time(NULL);
int cls_equipos::static_clsregistros_mbtcp[20] = {};
int cls_equipos::static_clsdatosleidos_mbtcp[20] = {};

/////////////////////////////////////
Logger logger;

////////////////////////////////////



cls_equipos::cls_equipos() {

	id = 0; puerto = 502; direccion_esclavo = 1; velocidad = 9600;
	bitsparada = 1; habilitar = false; registro = 0; escalamiento = 1;
	muestreo = 0; periodo = 1; bit1 = -1; cantidad1 = 1;
	solocambios = 0; registrable = 0; resultado_leer = false;
	tiempotranscurridoerrores = 0; ultima_llamada_leer = 0; n_buf = 0;
	ultimo_muestreo = 0; // ultimo muestreo para la lecturas y alarmas
	ultimo_insertar = 0; // ultimo comando insertar en base de datos tbvalores
	ultimo_insertar_ex = 0; // ultimo comando insertar en bd externa
	ultima_escritura_tbinstantaneo = 0;
	html_final = "</div>";
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
	buffer_owner.reset(new unsigned char[tamanobuffer + 1](), std::default_delete<unsigned char[]>());
	buf = buffer_owner.get();
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
	std::lock_guard<std::recursive_mutex> state(*state_mutex);
	resultado_leer = false;
	string mensajedeerror = "";
	

	resultadoleer_rc = -1;
    ReadMeasurement measurement(resultadoleer_rc);
    MonitorRead monitor_read(*this);
    register_acquired() = 0;
    register_acquired_steady() = monitor::Clock::time_point{};
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
		*suspension_logged = false;
	}

	if (*pcontador_errores > cuenta_errores_maximo) {
		mensajedeerror = "se suspende dispositivo " + nombre + " por exceso de error y ";
		mensajedeerror += "se reanuda dentro de " + cast_double_to_string(tiemporeset_cuenta_errores - *ptiempotranscurridoerrores) + " segundos";
		imprimir_mensaje_importante(mensajedeerror, rojo);//Loggin
		if(!*suspension_logged){logger.registrarError("INFO: Excess errors. Device "+nombre+" suspended.");*suspension_logged=true;}
		resultadoleer_rc = -2;
		return resultadoleer_rc;
	}
	imprimir_mensaje("leer: " + tag, negro);
	
	if ((protocolo.compare("MBTCP") == 0)) {
		leer_mbtcp();
	}
	else if (protocolo.compare("MBSERIAL") == 0) {
		leer_mbserial();
	}
	else if (protocolo.compare("RS232") == 0) {
		//cout << " lectura rs232 ";
		leer_rs232();
		
	};


	resultado_leer = resultadoleer_rc > 0;
	if (resultadoleer_rc > 0) {
        ultimo_muestreo = register_acquired() ? register_acquired() : time(NULL);
        SampleValue sample;
        sample.sequence = ++sample_sequence;
        sample.acquired = ultimo_muestreo;
        sample.value = clsvalorfloat;
        sample.text = clsvalorstr;
        latest_sample = sample;
        sample_window.observe(sample);
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
    uint64_t valor_long;
    uint16_t valor;
    char buffer[32];
    stringstream ssdatos;
    int rc = -1;

    static_clsnombre_mbtcp = nombre;
    auto lease = ModbusLease::tcp(direccion_ip, puerto);
    ctx_mbtcp = lease.get();
    rastro = nombre + ";" + direccion_ip + ":" + cast_int_to_string(puerto) + ";" + tag + ";" + "idslave=" + cast_int_to_string(direccion_esclavo) + ";reg=" + cast_int_to_string(registro);

    if (ctx_mbtcp == NULL) {
        fprintf(stderr, "=falla en iniciar libmodbus\n");
        logger.registrarError("DEBUG: libmodbus fail start in: leer_mbtcp() : " + std::string(__FILE__));
        conectadocondispositivo = false;
        resultado_leer = false;
        resultadoleer_rc = -1;
        return;
    }

    if (direccion_esclavo == 0) direccion_esclavo = 1;
    modbus_set_slave(ctx_mbtcp, direccion_esclavo);

    if (lease.connect(G_modbus_timeout) == -1) {
        if (errno == EAGAIN) { resultadoleer_rc = -3; resultado_leer = false; return; }
        string smsmodbus = string(modbus_strerror(errno));
        mensajedeerror = "fallo en modbus errno= " + smsmodbus + " ";
        mensajedeerror += rastro;
        imprimir_mensaje(mensajedeerror, rojo); //Loggin
        logger.registrarError("WARN: Modbus TCP read fail."+smsmodbus+" "+rastro);
        lease.invalidate();
        conectadocondispositivo = false;
        resultado_leer = false;
        rc = -1;
    }

    // lee los holding registers
    rc = -1;
    intento = 0;
    cantidad1 = std::max(1, std::min(cantidad1, 20));
    //modbus_set_response_timeout(ctx_mbtcp, 0, 200000);

    while ((intento < intentosmax) && driver_running() && (rc == -1) && (conectadocondispositivo == true)) {
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
                rc = read_cached_registers(lease, 3, registro, 2, datos);
                valor_float = cast_get_float_abcd(datos);
                snprintf(buffer, sizeof(buffer), "%f", valor_float);
            }

            else if (interpretacion.compare("float32badc") == 0)
            {
                rc = read_cached_registers(lease, 3, registro, 2, datos);
                valor_float = cast_get_float_badc(datos);
                snprintf(buffer, sizeof(buffer), "%f", valor_float);
            }

            else if (interpretacion.compare("float32cdab") == 0)
            {
                rc = read_cached_registers(lease, 3, registro, 2, datos);
                valor_float = cast_get_float_cdab(datos);
                snprintf(buffer, sizeof(buffer), "%f", valor_float);
            }

            else if (interpretacion.compare("float32dcba") == 0)
            {
                rc = read_cached_registers(lease, 3, registro, 2, datos);
                valor_float = cast_get_float_dcba(datos);
                snprintf(buffer, sizeof(buffer), "%f", valor_float);
                imprimir_mensaje("**"+cast_float_to_string(valor_float), negro);
            }

            else if (interpretacion.compare("float32fp") == 0)
            {
                // solo para los PM factor de potencia
                rc = read_cached_registers(lease, 3, registro, 2, datos);
                valor_float = cast_get_float_dcba(datos);
                valor_float = convert_float_fp(valor_float);
                snprintf(buffer, sizeof(buffer), "%f", valor_float);
            }

            else if (interpretacion.compare("longle") == 0)
            {
                rc = read_cached_registers(lease, 3, registro, 2, datos);
                valor_long = decode_u32_le(datos);
                snprintf(buffer, sizeof(buffer), "%" PRIu64, valor_long);
            }

            else if (interpretacion.compare("longle64") == 0)
            {
                rc = read_cached_registers(lease, 3, registro, 4, datos);
                valor_long = decode_u64_le(datos);
                snprintf(buffer, sizeof(buffer), "%" PRIu64, valor_long);
            }

            else if (interpretacion.compare("longbe") == 0)
            {
                rc = read_cached_registers(lease, 3, registro, 2, datos);
                valor_long = decode_u32_be(datos);
                snprintf(buffer, sizeof(buffer), "%" PRIu64, valor_long);
            }

            else if (interpretacion.compare("word") == 0)
            {
                rc = read_cached_registers(lease, 3, registro, cantidad1, datos);

                if ((bit1 >= 0) && (bit1 <= 15)) {
                    valor = obtenervalorbit(datos[0], bit1);
                }
                else {
                    valor = datos[0];
                }

                snprintf(buffer, sizeof(buffer), "%i", valor);
            }

            // ========================================================
            // NUEVA INTERPRETACION: WORD CON SIGNO (INT16)
            // ========================================================

            else if (interpretacion.compare("word_signed") == 0)
            {
                rc = read_cached_registers(lease, 3, registro, cantidad1, datos);

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
                rc = read_cached_registers(lease, 4, registro, 2, datos);
                valor_float = cast_get_float_abcd(datos);
                snprintf(buffer, sizeof(buffer), "%f", valor_float);
            }

            else if (interpretacion.compare("float32badc") == 0)
            {
                rc = read_cached_registers(lease, 4, registro, 2, datos);
                valor_float = cast_get_float_badc(datos);
                snprintf(buffer, sizeof(buffer), "%f", valor_float);
            }

            else if (interpretacion.compare("float32cdab") == 0)
            {
                rc = read_cached_registers(lease, 4, registro, 2, datos);
                valor_float = cast_get_float_cdab(datos);
                snprintf(buffer, sizeof(buffer), "%f", valor_float);
            }

            else if (interpretacion.compare("float32dcba") == 0)
            {
                rc = read_cached_registers(lease, 4, registro, 2, datos);
                valor_float = cast_get_float_dcba(datos);
                snprintf(buffer, sizeof(buffer), "%f", valor_float);
            }

            else if (interpretacion.compare("longle") == 0)
            {
                rc = read_cached_registers(lease, 4, registro, 2, datos);
                valor_long = decode_u32_le(datos);
                snprintf(buffer, sizeof(buffer), "%" PRIu64, valor_long);
            }

            else if (interpretacion.compare("longbe") == 0)
            {
                rc = read_cached_registers(lease, 4, registro, 2, datos);
                valor_long = decode_u32_be(datos);
                snprintf(buffer, sizeof(buffer), "%" PRIu64, valor_long);
            }

            else if (interpretacion.compare("word") == 0)
            {
                rc = read_cached_registers(lease, 4, registro, cantidad1, datos);

                if ((bit1 >= 0) && (bit1 <= 15)) {
                    valor = obtenervalorbit(datos[0], bit1);
                }
                else {
                    valor = datos[0];
                }

                snprintf(buffer, sizeof(buffer), "%i", valor);
            }

            // ========================================================
            // NUEVA INTERPRETACION: WORD CON SIGNO (INT16)
            // ========================================================

            else if (interpretacion.compare("word_signed") == 0)
            {
                rc = read_cached_registers(lease, 4, registro, cantidad1, datos);

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
            snprintf(buffer, sizeof(buffer), "%i", datos8b[0]);
            memcpy(datos, datos8b, 4);
        }

        // ============================================================
        // DISCRETE INPUTS (ir)
        // ============================================================

        if (funcion.compare("ir") == 0) {
            rc = modbus_read_input_bits(ctx_mbtcp, registro, 1, datos8b);
            snprintf(buffer, sizeof(buffer), "%i", datos8b[0]);
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

        for (int k = 0; k < 20; ++k) static_clsdatosleidos_mbtcp[k] = datos[k];

        for (int offset1 = 0; offset1 < 20; offset1++) {
            if (offset1 >= cantidad1) {
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

    if (rc < 0) lease.invalidate();

    resultadoleer_rc = rc;
};


 // -------------------------------------------------------------------------

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
    uint64_t valor_long;
    uint16_t valor;
    char buffer[32];
    stringstream ssdatos;
    int rc = -1;

    static_clsnombre_mbserial = nombre;
    auto lease = ModbusLease::rtu(interface1, velocidad, paridad.empty() ? 'N' : paridad[0], numerodebits, bitsparada);
    ctx_mbserial = lease.get();
    rastro = nombre + ";" + interface1 + ";" + "idslave=" + cast_int_to_string(direccion_esclavo) + ";" + tag + ";" + "reg=" + cast_int_to_string(registro);

    if (ctx_mbserial == NULL) {
        fprintf(stderr, "=falla en iniciar libmodbus\n");//Loggin
        logger.registrarError("DEBUG: libmodbus fail start in: leer_mbserial() : " + std::string(__FILE__));
        conectadocondispositivo = false;
        resultado_leer = false;
        resultadoleer_rc = -1;
        return;
    }

    if (direccion_esclavo == 0) direccion_esclavo = 1;
    modbus_set_slave(ctx_mbserial, direccion_esclavo);

    if (lease.connect(G_modbus_timeout) == -1) {
        if (errno == EAGAIN) { resultadoleer_rc = -3; resultado_leer = false; return; }
        string smsmodbus = string(modbus_strerror(errno));
        mensajedeerror = "fallo en modbus errno= " + smsmodbus + " ";//Loggin
        mensajedeerror += rastro;
        imprimir_mensaje(mensajedeerror, rojo);
        logger.registrarError("WARN: Modbus serial fail." + smsmodbus + " " + rastro);
        lease.invalidate();
        conectadocondispositivo = false;
        resultado_leer = false;
        rc = -1;
    }

    // lee los holding registers
    rc = -1;
    intento = 0;
    cantidad1 = std::max(1, std::min(cantidad1, 20));
    //modbus_set_response_timeout(ctx_mbserial, 0, 200000);

    while ((intento < intentosmax) && driver_running() && (rc == -1) && (conectadocondispositivo == true)) {

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
                rc = read_cached_registers(lease, 3, registro, 2, datos);
                valor_float = cast_get_float_abcd(datos);
                snprintf(buffer, sizeof(buffer), "%f", valor_float);
            }

            else if (interpretacion.compare("float32badc") == 0)
            {
                rc = read_cached_registers(lease, 3, registro, 2, datos);
                valor_float = cast_get_float_badc(datos);
                snprintf(buffer, sizeof(buffer), "%f", valor_float);
            }

            else if (interpretacion.compare("float32cdab") == 0)
            {
                rc = read_cached_registers(lease, 3, registro, 2, datos);
                valor_float = cast_get_float_cdab(datos);
                snprintf(buffer, sizeof(buffer), "%f", valor_float);
            }

            else if (interpretacion.compare("float32dcba") == 0)
            {
                rc = read_cached_registers(lease, 3, registro, 2, datos);
                valor_float = cast_get_float_dcba(datos);
                snprintf(buffer, sizeof(buffer), "%f", valor_float);
            }

            else if (interpretacion.compare("float32fp") == 0)
            {
                // solo para los PM factor de potencia
                rc = read_cached_registers(lease, 3, registro, 2, datos);
                valor_float = cast_get_float_dcba(datos);
                valor_float = convert_float_fp(valor_float);
                snprintf(buffer, sizeof(buffer), "%f", valor_float);
            }

            else if (interpretacion.compare("longle") == 0)
            {
                rc = read_cached_registers(lease, 3, registro, 2, datos);
                valor_long = decode_u32_le(datos);
                snprintf(buffer, sizeof(buffer), "%" PRIu64, valor_long);
            }

            else if (interpretacion.compare("longle64") == 0)
            {
                rc = read_cached_registers(lease, 3, registro, 4, datos);
                valor_long = decode_u64_le(datos);
                snprintf(buffer, sizeof(buffer), "%" PRIu64, valor_long);
            }

            else if (interpretacion.compare("longbe") == 0)
            {
                rc = read_cached_registers(lease, 3, registro, 2, datos);
                valor_long = decode_u32_be(datos);
                snprintf(buffer, sizeof(buffer), "%" PRIu64, valor_long);
            }

            else if (interpretacion.compare("word") == 0)
            {
                rc = read_cached_registers(lease, 3, registro, cantidad1, datos);

                if ((bit1 >= 0) && (bit1 <= 15)) {
                    valor = obtenervalorbit(datos[0], bit1);
                }
                else {
                    valor = datos[0];
                }

                snprintf(buffer, sizeof(buffer), "%i", valor);
            }

            // ========================================================
            // NUEVA INTERPRETACION: WORD CON SIGNO (INT16)
            // ========================================================

            else if (interpretacion.compare("word_signed") == 0)
            {
                rc = read_cached_registers(lease, 3, registro, cantidad1, datos);

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
                rc = read_cached_registers(lease, 4, registro, 2, datos);
                valor_float = cast_get_float_abcd(datos);
                snprintf(buffer, sizeof(buffer), "%f", valor_float);
            }

            else if (interpretacion.compare("float32badc") == 0)
            {
                rc = read_cached_registers(lease, 4, registro, 2, datos);
                valor_float = cast_get_float_badc(datos);
                snprintf(buffer, sizeof(buffer), "%f", valor_float);
            }

            else if (interpretacion.compare("float32cdab") == 0)
            {
                rc = read_cached_registers(lease, 4, registro, 2, datos);
                valor_float = cast_get_float_cdab(datos);
                snprintf(buffer, sizeof(buffer), "%f", valor_float);
            }

            else if (interpretacion.compare("float32dcba") == 0)
            {
                rc = read_cached_registers(lease, 4, registro, 2, datos);
                valor_float = cast_get_float_dcba(datos);
                snprintf(buffer, sizeof(buffer), "%f", valor_float);
            }

            else if (interpretacion.compare("longle") == 0)
            {
                rc = read_cached_registers(lease, 4, registro, 2, datos);
                valor_long = decode_u32_le(datos);
                snprintf(buffer, sizeof(buffer), "%" PRIu64, valor_long);
            }

            else if (interpretacion.compare("longbe") == 0)
            {
                rc = read_cached_registers(lease, 4, registro, 2, datos);
                valor_long = decode_u32_be(datos);
                snprintf(buffer, sizeof(buffer), "%" PRIu64, valor_long);
            }

            else if (interpretacion.compare("word") == 0)
            {
                rc = read_cached_registers(lease, 4, registro, cantidad1, datos);

                if ((bit1 >= 0) && (bit1 <= 15)) {
                    valor = obtenervalorbit(datos[0], bit1);
                }
                else {
                    valor = datos[0];
                }

                snprintf(buffer, sizeof(buffer), "%i", valor);
            }

            // ========================================================
            // NUEVA INTERPRETACION: WORD CON SIGNO (INT16)
            // ========================================================

            else if (interpretacion.compare("word_signed") == 0)
            {
                rc = read_cached_registers(lease, 4, registro, cantidad1, datos);

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
            snprintf(buffer, sizeof(buffer), "%i", datos8b[0]);
            memcpy(datos, datos8b, 4);
        }

        // ============================================================
        // DISCRETE INPUTS (ir)
        // ============================================================

        if (funcion.compare("ir") == 0) {
            rc = modbus_read_input_bits(ctx_mbserial, registro, 1, datos8b);
            snprintf(buffer, sizeof(buffer), "%i", datos8b[0]);
            memcpy(datos, datos8b, 4);
        }

    } // fin del while

    // ================================================================
    // RESULTADO DE LA LECTURA
    // ================================================================

    if (rc != -1) {

        for (int k = 0; k < 20; ++k) static_clsdatosleidos_mbserial[k] = datos[k];

        for (int offset1 = 0; offset1 < 20; offset1++) {
            if (offset1 >= cantidad1) {
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

    if (rc < 0) lease.invalidate();

    resultadoleer_rc = rc;
};


//-------------------------------------

void cls_equipos::leer_rs232(void) {
	std::lock_guard<std::mutex> serial(*buffer_mutex);

	resultadoleer_rc = -1;
    if (tamanoventana < 0) return;
    const size_t largoventana = static_cast<size_t>(tamanoventana);
	string tmpstr = "";
	string token = "";
	size_t pos1, pos2;
	vector<stdelim> vector_stdelim;
	int longitudtoken = 0;
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


void cls_equipos::esperar(double segundos) { wait_driver_seconds(segundos); }





void cls_equipos::setvalorfloat(float nuevovalor) {

	oldclsvalorfloat = clsvalorfloat;
	oldclsvalor = clsvalor;
	oldclsvalorstr = clsvalorstr;

	clsvalorfloat =  nuevovalor*escalamiento;
	clsvalor = numeric_to_int(clsvalorfloat);
	clsvalorstr = cast_float_to_string(clsvalorfloat);

	if (oldclsvalorfloat == clsvalorfloat) {
		fuecambiado = false;
	}
	else {
		++change_revision;
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
	clsvalor = numeric_to_int(clsvalorfloat);
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
		++change_revision;
		fuecambiado = true;
		fueescritodb = false;
	}

	
	return;
}
