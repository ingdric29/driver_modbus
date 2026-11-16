// Automonitor - separated from main.cpp; public function contracts unchanged.
// Original implementation: Alejandro Diomede / Automatroni.
#include "include/driver_services.h"
#include "include/conversiones.h"
#include "include/runtime_control.h"
#include <algorithm>
#include <cstdlib>
#include <ctime>
#include <iostream>
#include <cctype>
#include "include/rs232.h"

using namespace std;

void lectura_puerto_serial(cls_equipos *equipo_serial) {
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

            if (RS232_OpenComport(numpuertoserial, velocidadserial, modo,
                                  controldeflujo)) // retorna 1 en caso de error
            {
                //falla en abrir puerto;
                imprimir_mensaje("falla en abrir puerto: (" + nombrepuerto + ")", rojo);
                logger_main.registrarError("ERROR: No se pudo abrir el puerto: (" + nombrepuerto +
                                           ")");

            } else {
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
            cout << "[ AVISO ] Reintento del puerto serie en 10 s (" << nombrepuerto << ") "
                 << endl;
            esperar(10);
        } // primer while 1

        return;
    } catch (...) {
        cout << "[ ERROR ] Fallo en el modulo RS232." << endl;

        std::string mensajeError = std::string("DEBUG: RS232 module fail!") +
                                   " | File: " + __FILE__ + " | Function: " + __FUNCTION__ +
                                   " | Line: " + std::to_string(__LINE__);

        logger_main.registrarError(mensajeError);
        return;
    }
}
