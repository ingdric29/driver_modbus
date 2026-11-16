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
    cls_equipos &tag;
    monitor::Clock::time_point start = monitor::Clock::now();
    explicit MonitorRead(cls_equipos &t) : tag(t) {}
    ~MonitorRead() {
        auto us =
            std::chrono::duration_cast<std::chrono::microseconds>(monitor::Clock::now() - start)
                .count();
        monitor::State::instance().publish(
            tag.monitor_key, tag.resultadoleer_rc, tag.clsvalorstr, tag.latest_sample.acquired, us,
            register_acquired_steady() == monitor::Clock::time_point{}
                ? monitor::Clock::now()
                : register_acquired_steady());
    }
};
} // namespace
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

    id = 0;
    puerto = 502;
    direccion_esclavo = 1;
    velocidad = 9600;
    bitsparada = 1;
    habilitar = false;
    registro = 0;
    escalamiento = 1;
    muestreo = 0;
    periodo = 1;
    bit1 = -1;
    cantidad1 = 1;
    solocambios = 0;
    registrable = 0;
    resultado_leer = false;
    tiempotranscurridoerrores = 0;
    ultima_llamada_leer = 0;
    n_buf = 0;
    ultimo_muestreo = 0;    // ultimo muestreo para la lecturas y alarmas
    ultimo_insertar = 0;    // ultimo comando insertar en base de datos tbvalores
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
    tamanobuffer = (int)tamanoventana * 2;
    buffer_owner.reset(new unsigned char[tamanobuffer + 1](),
                       std::default_delete<unsigned char[]>());
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
        *ptiempotranscurridoerrores =
            *ptiempotranscurridoerrores + difftime(time(NULL), *pultima_llamada_leer);
    } else {
        *ptiempotranscurridoerrores = 0.0;
    }
    *pultima_llamada_leer = time(NULL);

    if (*ptiempotranscurridoerrores > tiemporeset_cuenta_errores) {
        *pcontador_errores = cuenta_errores_maximo / 2;
        mensajedeerror = "se activa dispositivo " + nombre + " para verificar ";
        mensajedeerror += "si hay conexion ";
        imprimir_mensaje_importante(mensajedeerror, verde);
        logger.registrarError("INFO: Verificacion de comunicacion. Dispositivo " + nombre +
                              " habilitado para comprobar la conexion.");
        *suspension_logged = false;
    }

    if (*pcontador_errores > cuenta_errores_maximo) {
        mensajedeerror = "se suspende dispositivo " + nombre + " por exceso de error y ";
        mensajedeerror +=
            "se reanuda dentro de " +
            cast_double_to_string(tiemporeset_cuenta_errores - *ptiempotranscurridoerrores) +
            " segundos";
        imprimir_mensaje_importante(mensajedeerror, rojo); //Loggin
        if (!*suspension_logged) {
            logger.registrarError("AVISO: Exceso de errores. Dispositivo " + nombre +
                                  " suspendido.");
            *suspension_logged = true;
        }
        resultadoleer_rc = -2;
        return resultadoleer_rc;
    }
    imprimir_mensaje("leer: " + tag, negro);

    if ((protocolo.compare("MBTCP") == 0)) {
        leer_mbtcp();
    } else if (protocolo.compare("MBSERIAL") == 0) {
        leer_mbserial();
    } else if (protocolo.compare("RS232") == 0) {
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
    } else {
        ultimo_muestreo = time(NULL) - (periodo / 2);
    }

    if (*pcontador_errores < (cuenta_errores_maximo / 2)) {
        *pestadocomunicacion = true;
    } else {
        *pestadocomunicacion = false;
    }

    return resultadoleer_rc;
}

//---------------------------------------------------------------------

//---------------------------------------------------------------------
//leer_mbtcp() con nueva interpretacion de valores Word con signo

;

// -------------------------------------------------------------------------

//-------------
//leer_mbserial() Variables tipo Word con signo

;

//-------------------------------------

;

/* Get a float from 4 bytes (Modbus) in inversed format (DCBA) */
float cls_equipos::convert_float_fp(float regVal) {
    float PF_Val;

    if (regVal > 1) {
        PF_Val = 2 - regVal;
        //PF is leading
    } else if (regVal < -1) {
        PF_Val = -2 - regVal;
        //PF is leading
    } else if (regVal == 1.0) {
        PF_Val = regVal;
        //PF is at unity
    } else {
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

    clsvalorfloat = nuevovalor * escalamiento;
    clsvalor = numeric_to_int(clsvalorfloat);
    clsvalorstr = cast_float_to_string(clsvalorfloat);

    if (oldclsvalorfloat == clsvalorfloat) {
        fuecambiado = false;
    } else {
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
    } else {
        clsvalorstr = cast_float_to_string(clsvalorfloat);
    }

    if (oldclsvalorstr.compare(clsvalorstr) == 0) {
        fuecambiado = false;

    } else {
        ++change_revision;
        fuecambiado = true;
        fueescritodb = false;
    }

    return;
}
