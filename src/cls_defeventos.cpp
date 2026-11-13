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
}

int cls_defeventos::cargar_en_memoriaram() {
	int res;
	res = actualizar_defeventos(&stdblocal);
	return res;
}

// ubica el registro en la tabla de cls_equipos
// de manera de ubicarlo mas rapido
int cls_defeventos::cargar_posicion() {
	int ind = 0;
	indiceenram = -1;
	while (ind < GLOBAL_contador_equipos_mbtcp) {
		if ((nombredispositivo.compare(equipo_mbtcp[ind].nombre) == 0) &&
			(tag.compare(equipo_mbtcp[ind].tag) == 0)) {
			indiceenram = ind;
			protocolo = "MBTCP";
			if ((bit1>=0) && (bit1<=15)) {
				valoractual = (equipo_mbtcp[ind].clsvalor >> bit1) & 0x1;
			}
			else {
				valoractual = equipo_mbtcp[ind].clsvalor;
			}
			pequipo = &equipo_mbtcp[ind];
			
		}
		ind++;
	}
	if (ind != -1) return indiceenram;
	while (ind < GLOBAL_contador_equipos_mbserial) {
		if ((nombredispositivo.compare(equipo_mbserial[ind].nombre) == 0) &&
			(tag.compare(equipo_mbserial[ind].tag) == 0)) {
			indiceenram = ind;
			protocolo = "MBSERIAL";
			if ((bit1 >= 0) && (bit1 <= 15)) {
				valoractual = (equipo_mbtcp[ind].clsvalor >> bit1) & 0x1;
				valoranterior = valoractual;
			}
			else {
				valoractual = equipo_mbtcp[ind].clsvalor;
				valoranterior = valoractual;
			}
			pequipo = &equipo_mbtcp[ind];
		}
		ind++;
	}
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
	
	if (indiceenram != -1) {
		diferencia = difftime(time(NULL), tiempoanterior);
		tiempoactual = time(NULL);
		if ((bit1 >= 0) && (bit1 <= 15)) {
			valoractual = (pequipo->clsvalor >> bit1) & 0x1;
		
		}
		else {
			valoractual = pequipo->clsvalor;
		
		}
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
