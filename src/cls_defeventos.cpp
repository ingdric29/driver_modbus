//Modificación hecha por: Ing. Javier Flores 12/11/24

#include "include/cls_defeventos.h"
#include "include/globales.h"
#include "include/conversiones.h"

using namespace std;

extern stdatabase stdblocal;
extern stdatabase stdbexterna;

// metodo constructor
cls_defeventos::cls_defeventos() {
	valoractual = 0;
	valoranterior = 0;
	tiempoactual = 0;
	tiempoanterior = 0;
	indiceenram = -1; pequipo = nullptr;
}

int cls_defeventos::cargar_en_memoriaram() {
	int res;
	res = actualizar_defeventos(&stdblocal);
	return res;
}

// ubica el registro en la tabla de cls_equipos
// de manera de ubicarlo mas rapido
int cls_defeventos::cargar_posicion() {
    indiceenram = -1; pequipo = nullptr;
    auto find = [&](cls_equipos* tags, int count, const std::string& protocol) {
        for (int i = 0; i < count; ++i) if (tags[i].nombre == nombredispositivo && tags[i].tag == tag) {
            std::lock_guard<std::recursive_mutex> state(*tags[i].state_mutex);
            indiceenram = i; pequipo = &tags[i]; protocolo = protocol;
            valoractual = bit1 >= 0 && bit1 <= 15 ? (tags[i].clsvalor >> bit1) & 1 : tags[i].clsvalor;
            valoranterior = valoractual;
            return true;
        }
        return false;
    };
    if (!find(equipo_mbtcp, GLOBAL_contador_equipos_mbtcp, "MBTCP")
        && !find(equipo_mbserial, GLOBAL_contador_equipos_mbserial, "MBSERIAL"))
        find(equipo_rs232, GLOBAL_contador_equipos_rs232, "RS232");
    return indiceenram;
}



// lee el valor del tag en ram

int cls_defeventos::leer_valorenram() {
	double diferencia = 0.0;
	int word;
	uint16_t x1, x2, x3;
	string campost[8];
	string valorst[8];
	campost[0] = "id_defeventos";	valorst[0] = cast_int_to_string(id);
	campost[1] = "fechatiempo";		valorst[1] = cast_time_to_datetime(time(NULL));
	campost[2] = "tiempo";			valorst[2] = cast_time_to_datetime(time(NULL));
	campost[3] = "concentrador";	valorst[3] = GLOBAL_nombre_concentrador;
	campost[4] = "nombredispositivo"; valorst[4] = nombredispositivo;
	campost[5] = "tag";				valorst[5] = tag;
	campost[6] = "bit1";	
	campost[7] = "mensaje";			
	
	if (indiceenram != -1 && pequipo) {
        {
        std::lock_guard<std::recursive_mutex> state(*pequipo->state_mutex);
        if (!pequipo->resultado_leer) return -1;
		diferencia = difftime(time(NULL), tiempoanterior);
		tiempoactual = time(NULL);
		if ((bit1 >= 0) && (bit1 <= 15)) {
			valoractual = (pequipo->clsvalor >> bit1) & 0x1;
		
		}
		else {
			valoractual = pequipo->clsvalor;
		
		}
        } // Snapshot complete: SQL below does not hold the device mutex.
		//esta instruccion solo se ejecuta una vez al iniciar programa
		if ((diferencia > 86400.0) && (GLOBAL_estado_primera_ejecucion_programa==3)) {
			valoranterior = valoractual;
			tiempoanterior = tiempoactual;
			return 0;
		}
		if (valoranterior != valoractual) {
			// aqui van las acciones cuando se detecta cambio de valor
			//cout << "detectado cambio " << pequipo->nombre << " - " << pequipo->tag << " - " << pequipo->clsvalor << endl;
			x1 = (uint16_t) valoranterior;
			x2 = (uint16_t) valoractual;
			x3 = x1 ^ x2;
			if (x3 > 0) {
				if ((x2 == 1) && (bit1>=0) && (bit1 <=15)) {
						//se activo
						valorst[6] = cast_int_to_string(bit1);
						valorst[7] = activado + " - (" + cast_int_to_string(valoractual) + ")";
				}
				else {
						//se desactivo
						valorst[6] = cast_int_to_string(bit1);
						valorst[7] = desactivado + " - (" + cast_int_to_string(valoractual) + ")";
				}
				//int insertar_valor(struct stdatabase* db, string tabla, string* campo, string* valor, int cantidad)
				insertar_valor(&stdblocal, "tbhistoeventos", &campost[0], &valorst[0], 8);
			}
		}
	
		valoranterior = valoractual;
		tiempoanterior = tiempoactual;
		return 1;
	}
	else {
		return 0;
	}

	
}
