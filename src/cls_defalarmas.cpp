/*
programa dedarrollado por ing. Alejandro diomede - Automatroni - email: diomede3009@gmail.com
Define la estructura de datos llamada Equipos - tcp y serial
Modificación hecha por: Ing. Javier Flores 12/11/24
*/

#include "include/conversiones.h"
#include <errno.h>
#include <vector>
#include <stack>
#include <ctype.h>
#include "include/cls_defalarmas.h"
#include "include/globales.h"
#include "include/estructuras.h"
#include <sstream>
#include "include/alarm_expression.h"

using namespace std;

extern stdatabase stdblocal;
extern stdatabase stdbexterna;

// metodo constructor
cls_defalarmas::cls_defalarmas() {
    id = 0; activa = false; reconocida = false;
	expresionactivar = "0";
	expresiondesactivar = "0";
	ejecutado_activar_cmd = false;
	ejecutado_desactivar_cmd = true;
	ejecutado_activar_email = false;
	ejecutado_desactivar_email = true;
	ejecutado_activar_sms = false;
	ejecutado_desactivar_sms = true;
	ejecutado_activar_bd = false;
	ejecutado_desactivar_bd = true;
	ejecutado_activar = false;
	ejecutado_desactivar = true;
	contador_de_acciones_activar = 0;
	contador_de_acciones_desactivar = 0;
	contador_de_acciones_max = 10;
}


// reemplaza los simbolos por valores
string cls_defalarmas::reemplazar_simbolos(const string& expression) {
    std::string output; size_t begin = 0;
    while (begin < expression.size()) {
        const size_t marker = expression.find("$(", begin);
        if (marker == std::string::npos) { output += expression.substr(begin); break; }
        output += expression.substr(begin, marker - begin);
        const size_t end = expression.find(')', marker + 2);
        if (end == std::string::npos) throw std::invalid_argument("unclosed alarm tag reference");
        const auto reference = expression.substr(marker + 2, end - marker - 2);
        const size_t dot = reference.find('.');
        if (dot == std::string::npos || dot == 0 || dot + 1 == reference.size()) throw std::invalid_argument("invalid alarm tag reference");
        stres sample{};
        if (leertag2(reference.substr(0, dot), reference.substr(dot + 1), &sample) != 0)
            throw std::invalid_argument("unknown or unavailable alarm tag");
        output += cast_double_to_string(sample.valorfloat);
        begin = end + 1;
    }
    return output;
}

// dado el dispositivo y el tag obtiene el valor


// dado el dispositivo y el tag obtiene el valor.  retorna 0, si todo ok, -1 si hay error
int cls_defalarmas::leertag2 (const string& dispositivo, const string& tag, struct stres * local ) {

	bool  encontrado = false;
	for (int i=0; i<clscantidad_equipo_mbtcp; i++) {
	  if (clsequipo_mbtcp) {
		if ((clsequipo_mbtcp[i].nombre.compare(dispositivo) ==0) && (clsequipo_mbtcp[i].tag.compare(tag) ==0)) {
            std::lock_guard<std::recursive_mutex> state(*clsequipo_mbtcp[i].state_mutex);
            if (!clsequipo_mbtcp[i].resultado_leer) return -1;
			local->valorfloat = clsequipo_mbtcp[i].clsvalorfloat;
			local->valorstr = clsequipo_mbtcp[i].clsvalorstr;
			local->unidad = clsequipo_mbtcp[i].unidad;
			local->indice = i;
			local->puntero_equipo = &clsequipo_mbtcp[0];
			local->time_fechatiempo = clsequipo_mbtcp[i].ultimo_muestreo;
			clsorigen = clsequipo_mbtcp[i].direccion_ip;
			clsregistro = cast_int_to_string(clsequipo_mbtcp[i].registro);
			encontrado = true;
			break;
		}
	  }	
	}
	if (encontrado == false) {
	 for (int i=0; i<clscantidad_equipo_mbserial; i++) {
	   if (clsequipo_mbserial) {
		if ((clsequipo_mbserial[i].nombre.compare(dispositivo) ==0) && (clsequipo_mbserial[i].tag.compare(tag) ==0)) {
            std::lock_guard<std::recursive_mutex> state(*clsequipo_mbserial[i].state_mutex);
            if (!clsequipo_mbserial[i].resultado_leer) return -1;
			local->valorfloat = clsequipo_mbserial[i].clsvalorfloat;
			local->valorstr = clsequipo_mbserial[i].clsvalorstr;
			local->indice = i;
			local->puntero_equipo = &clsequipo_mbserial[0];
			local->time_fechatiempo = clsequipo_mbserial[i].ultimo_muestreo;
			clsorigen = clsequipo_mbserial[i].interface1;
			clsregistro = cast_int_to_string(clsequipo_mbserial[i].registro);
			encontrado = true;
			break;
		}
	  }
	 }
	}
	

	
	if (encontrado == false) {
		for (int i = 0; i < clscantidad_equipo_rs232; i++) {
			if (clsequipo_rs232) {
				if ((clsequipo_rs232[i].nombre.compare(dispositivo) == 0) && (clsequipo_rs232[i].tag.compare(tag) == 0)) {
            std::lock_guard<std::recursive_mutex> state(*clsequipo_rs232[i].state_mutex);
            if (!clsequipo_rs232[i].resultado_leer) return -1;
					local->valorfloat = clsequipo_rs232[i].clsvalorfloat;
					local->valorstr = clsequipo_rs232[i].clsvalorstr;
					local->indice = i;
					local->puntero_equipo = &clsequipo_rs232[0];
					local->time_fechatiempo = clsequipo_rs232[i].ultimo_muestreo;
					clsorigen = clsequipo_rs232[i].interface1;
					clsregistro = clsequipo_rs232[i].registro;
					encontrado = true;
					break;
				}
			}
		}
	}

	if (encontrado) {
		return 0;
	}
	else {
		return -1;
	}
	
	
}




string cls_defalarmas::reordenar_antes_evaluar(string in) { return alarm_postfix(in); }


int cls_defalarmas::evaluar1(string p) { return alarm_evaluate_postfix(p); }

string cls_defalarmas::eliminar_espacios(string p) {
	string pout = "";
	for (size_t i =0; i<p.size(); i++)
		if (p[i] != ' ') pout += p[i];
return pout;

}



int cls_defalarmas::evaluarcompleto() {
	string expresion_sinespacios1,expresion_sinespacios2;
	string expresion_numerica1,expresion_numerica2;
	string expresion_numerica_ordenada1,expresion_numerica_ordenada2;
	stringstream ss2;
	int resultadoactivar, resultadodesactivar;
	int resultado = 0;
	int res_ejecutar = 0;
	stres valortag1;
	int res_leertag2 = -1;
	try{
		
	res_leertag2 = leertag2 (clsdispositivo, clstag, &valortag1 );

	if ((res_leertag2 == 0) && (valortag1.time_fechatiempo > 0)) {
	// desactivar	
	expresion_sinespacios2 = eliminar_espacios(expresiondesactivar);
	ss2.str("");
	ss2 << "expresion desactivar: " << expresion_sinespacios2;
	imprimir_mensaje(ss2.str());
	expresion_numerica2 = reemplazar_simbolos(expresion_sinespacios2);
	expresiondesactivar_numerica = expresion_numerica2;
	ss2.str("");
	ss2 << "expresion desactivar numerica: " << expresiondesactivar_numerica;
	imprimir_mensaje(ss2.str());
	expresion_numerica_ordenada2 = reordenar_antes_evaluar(expresion_numerica2);
	ss2.str("");
	ss2 << "expresion_numerica_ordenada2 " << expresion_numerica_ordenada2;
	imprimir_mensaje(ss2.str());
	resultadodesactivar = evaluar1(expresion_numerica_ordenada2);
	ss2.str("");
	ss2 << "resultado desactivar " << resultadodesactivar;
	imprimir_mensaje(ss2.str());
	//activar
	expresion_sinespacios1 = eliminar_espacios(expresionactivar);
	//cout << "expresion activar: " << expresion_sinespacios << endl;
	ss2.str("");
	ss2 << "expresion activar sin espacios " << expresion_sinespacios1;
	imprimir_mensaje(ss2.str());
	expresion_numerica1 = reemplazar_simbolos(expresion_sinespacios1);
	expresionactivar_numerica = expresion_numerica1;
	ss2.str("");
	ss2 << "expresion activar numerica " << expresionactivar_numerica;
	imprimir_mensaje(ss2.str());
	//cout << "expresion activar numerica: " << expresion_numerica << endl;
	expresion_numerica_ordenada1 = reordenar_antes_evaluar(expresion_numerica1);
	ss2.str("");
	ss2 << "expresion activar numerica ordenada " << expresion_numerica_ordenada1;
	imprimir_mensaje(ss2.str());
	resultadoactivar = evaluar1(expresion_numerica_ordenada1);
	ss2.str("");
	ss2 << "resultado activar " << resultadoactivar;
	imprimir_mensaje(ss2.str());

	if ((resultadoactivar == 0) && (resultadodesactivar == 0)) {
		resultado = 0;
	}
	else if ((resultadoactivar == 0) && (resultadodesactivar == 1)) {
		resultado = 2;
		activa = false;
		contador_de_acciones_activar = 0;
		insertar_tbhistoalarmas();
		res_ejecutar = ejecutar_accion(acciondesactivar, activa);

	}
	else if ((resultadoactivar == 1) && (resultadodesactivar == 0)) {
		resultado = 1;
		activa = true;
		contador_de_acciones_desactivar = 0;
		insertar_tbhistoalarmas();
		res_ejecutar = ejecutar_accion(accionactivar, activa);

	}
	else if ((resultadoactivar == 1) && (resultadodesactivar == 1)) {
		resultado = 2;
		activa = false;
		contador_de_acciones_activar = 0;
		insertar_tbhistoalarmas();
		res_ejecutar = ejecutar_accion(acciondesactivar, activa);

	}
		
	return resultado;
	
	} // fin del if
	else {
		return -1;
	}
	
	} // fin del try
	
	catch(const std::exception& error) {
        imprimir_mensaje(std::string("Invalid alarm: ")+error.what(), rojo);
		imprimir_mensaje("falla - modulo cls_defalarmas :: evaluarcompleto");
		return -1;
	}
	
} 


// si es 0 todo ok, sino es un error
int cls_defalarmas::ejecutar_accion(const string& accionstr, bool alarmaactivada){
    std::lock_guard<std::recursive_mutex> commands(GLOBAL_command_mutex);
	size_t pos, pos_ant, inicio, fin;
	std::vector<string> vector_accion;
	int res       = 0; // si es 0 todo ok, sino es un error
	int res_cmd   = 0;
	int res_email = 0;
	int res_sms   = 0;

	pos = 0;
	pos_ant = 0;
	inicio = 0;
	fin =0;
	int indice = 0;
	string indicestr = "";
	bool ordenencontrada = false;
	vector_accion = split1(accionstr,';');
	string accion1 = "";
	stringstream ss1;
	stres valorAlarma;
	int res_leertag2;
	try {
	for (int i=0; i<vector_accion.size(); i++) {
		ordenencontrada = false;
		accion1 = vector_accion[i];
		pos = accion1.find("CMD[",pos_ant);
		if (pos != std::string::npos) { // consiguie la orden cmd
			ordenencontrada = true;
			inicio = pos;
			fin = accion1.find("]",inicio);
			if (fin != std::string::npos) { // busca el cierre del corchete
				pos = fin;
				indicestr = accion1.substr(inicio+4,fin-inicio-4); 
				ss1 << "comando CMD indice = " << cast_string_to_int(indicestr) << " longitud:" << indicestr.size(); 
				imprimir_mensaje(ss1.str());
				//cout << ss1.str() << endl;
				indice = cmd_buscarid(cast_string_to_int(indicestr));
				ss1.str("");
				ss1 << "indice en ram comando a ejecutar: " << indice; 
				//cout << ss1.str() << endl;
				imprimir_mensaje(ss1.str());
					if ((indice >= 0) && (indice < GLOBAL_contador_cmd)) {
						
						if (alarmaactivada == true) {
							
							if (ejecutado_activar_cmd == false) {
								//cout << "ejecutado cmd activar" << endl;
								res_cmd = cmd[indice].ejecutar();
							}
							
						}
						else {

							if (ejecutado_desactivar_cmd == false) {
								//cout << "ejecutado cmd desactivar" << endl;
								res_cmd = cmd[indice].ejecutar();

							}
							
						}
					


					}
			} 	// fin del cierre del corechete
			
		} // fin de la orden cmd
	
		if (ordenencontrada == false) {
			pos_ant = 0;	
			pos = accion1.find("EMAIL[",pos_ant);
			if (pos != std::string::npos) {
				ordenencontrada = true;
				inicio = pos;
				fin = accion1.find("]",inicio);
				if (fin != std::string::npos) { // busca el cierre del corchete
					pos = fin;
					indicestr = accion1.substr(inicio+6,fin-inicio-6); 
					//cout << "comando EMAIL indice = " << indicestr << " longitud:" << indicestr.size()<< endl;
					indice = email_buscarid(cast_string_to_int(indicestr));
					if (emaillist && (indice >= 0) && (indice < GLOBAL_contador_directorio)) {
                        cls_email notification = emaillist[indice];
                        notification.context = "alarma=" + std::to_string(id) + " dispositivo=" + clsdispositivo
                            + " directorio=" + indicestr + (alarmaactivada ? " evento=activar" : " evento=desactivar");
						//"activar   : " + expresionactivar + "[ " + expresionactivar_numerica + " ]"
						
						//"desactivar: " + expresiondesactivar + "[ " + expresiondesactivar_numerica + " ]"
						
						if (alarmaactivada == true) {
							//cout << "ejecurar accion por alarma activa" << endl;

							notification.asunto = "Alarma | " + descripcion + " | " + GLOBAL_nombre_concentrador;
						
							notification.mensaje = "Fecha y hora: " + fecha_para_emails_local_es();
							notification.mensaje1 = "Tipo: 	Alarma activada en " + GLOBAL_nombre_concentrador ;
							notification.mensaje2 = descripcion;
							//notification.mensaje3 = "Dispositivo	: " + clsdispositivo;
							//notification.mensaje4 = "Direccion ip / origen	: " + clsorigen;
							//notification.mensaje5 = "Registro				: " + clsregistro;
							//notification.mensaje6 = "Causa: " + expresionactivar + "[ " + expresionactivar_numerica + " ]";
							res_leertag2 = leertag2 (clsdispositivo, clsvalorAlarma, &valorAlarma );
							if 	((res_leertag2==0) && (valorAlarma.time_fechatiempo > 0)) {
								notification.mensaje3 = cast_double_to_string(valorAlarma.valorfloat,2);
								notification.mensaje4 = valorAlarma.unidad;
								}
							else {
								notification.mensaje3 = "";
								notification.mensaje4 = "";
								}
							
							
							if (ejecutado_activar_email == false) {
								res_email = notification.enviar();
							}
							
						}
						else {

							//cout << "ejecurar accion por alarma desactivada" << endl;
							notification.asunto = "Alarma desactivada | "  + descripcion + " | " + GLOBAL_nombre_concentrador;
							notification.mensaje = "Fecha y hora: " + fecha_para_emails_local_es();
							notification.mensaje1 = "Tipo: 	Alarma desactivada en " + GLOBAL_nombre_concentrador ;
							notification.mensaje2 = descripcion;
							//notification.mensaje3 = "Dispositivo	: " + clsdispositivo;
							//notification.mensaje4 = "Direccion ip / origen	: " + clsorigen;
							//notification.mensaje5 = "Registro				: " +  clsregistro;
							
							//notification.mensaje6 = "Causa: " + expresiondesactivar + "[ " + expresiondesactivar_numerica + " ]";
							res_leertag2 = leertag2 (clsdispositivo, clsvalorAlarma, &valorAlarma );
							if 	((res_leertag2==0) && (valorAlarma.time_fechatiempo > 0)) {
								notification.mensaje3 = cast_double_to_string(valorAlarma.valorfloat,2);
								notification.mensaje4 = valorAlarma.unidad;
								}
							else {
								notification.mensaje3 = "";
								notification.mensaje4 = "";
								}
							if (ejecutado_desactivar_email == false) {
								res_email = notification.enviar();

							}
							
						}
					


					} else {
                        res_email = -1;
                        imprimir_mensaje("No configured EMAIL recipient for ID " + indicestr, rojo);
                    }
					
				}	// fin del cierre del corechete
			
			}
		}

		if (ordenencontrada == false) {
			pos_ant = 0;
			pos = accion1.find("SMS[", pos_ant);
			if (pos != std::string::npos) {
				ordenencontrada = true;
				inicio = pos;
				fin = accion1.find("]", inicio);
				if (fin != std::string::npos) { // busca el cierre del corchete
					pos = fin;
					indicestr = accion1.substr(inicio + 4, fin - inicio - 4);
					imprimir_mensaje("se enviara sms. indice: " + indicestr);
					indice = sms_buscarid(cast_string_to_int(indicestr));
					imprimir_mensaje("el id del sms en ram es: " + cast_int_to_string(indice));
					if ((indice >= 0) && (indice < GLOBAL_contador_numerossms)) {
						if (alarmaactivada) {
							smslist[indice].clsasunto = "Alarma Activada [" + cast_time_to_datetime(time(NULL)) + "] - " + "condicion activar: " + expresionactivar + "[ " +expresionactivar_numerica + " ]" + " - " + descripcion;
							if (ejecutado_activar_sms == false) {
								res_sms = smslist[indice].enviar();
							}
							
						}
						else {
							
							smslist[indice].clsasunto = "Alarma Desactivada [" + cast_time_to_datetime(time(NULL)) + "] - " + "condicion desactivar: " + expresiondesactivar + "[ " + expresiondesactivar_numerica + " ]" + " - " + descripcion;
							if (ejecutado_desactivar_sms == false) {
								res_sms = smslist[indice].enviar();

							}
						}


					}
					
				}	// fin del cierre del corechete

			}
		}

	
	}


if (alarmaactivada == true) {
	ejecutado_desactivar_email = false;
	ejecutado_desactivar_cmd = false;
	ejecutado_desactivar_email = false;
		if (ejecutado_activar_email == false) {
			if (res_email == 0) {
				ejecutado_activar_email = true;
			}
		}
		if (ejecutado_activar_cmd == false) {
			if (res_cmd == 0) {
				ejecutado_activar_cmd = true;
			}
		}
		if (ejecutado_activar_sms == false) {
			if (res_sms == 0) {
				ejecutado_activar_sms = true;
			}
		}
}
else {
	ejecutado_activar_email = false;
	ejecutado_activar_cmd = false;
	ejecutado_activar_email = false;
		if (ejecutado_desactivar_email == false) {
			if (res_email == 0) {
				ejecutado_desactivar_email = true;
			}
		}
		if (ejecutado_desactivar_cmd == false) {
			if (res_cmd == 0) {
				ejecutado_desactivar_cmd = true;
			}
		}
		if (ejecutado_desactivar_sms == false) {
			if (res_sms == 0) {
				ejecutado_desactivar_sms = true;
			}
		}
}
			
	if ((res_cmd == 0) && (res_email==0) && (res_sms == 0)) {
		res = 0;
	}
	else {
		res = -1;
	}
	ss1.str("");
	ss1 << "resultado de la accion: " << res << " estado: " << alarmaactivada;
	imprimir_mensaje(ss1.str());
	
	return res;
	}

catch (...) {
	imprimir_mensaje("\033[1;31mfalla en el procedimiento cls_defalarmas::ejecutar_accion\033[0m ");
	return -1;
}
	
}	



int cls_defalarmas::insertar_tbhistoalarmas() {
	string arr_campos1[13];
	string arr_valores1[13];
	int res_insertardblocal = 0;
	int res_insertardbexterna = 0;
	int res_leertag2;
	stres valortag;
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
	valortag.valorstr = "0";
	if (activa) {
		ejecutado_desactivar_bd = false;
		
		if 	(ejecutado_activar_bd == false) {
			fechaocurrencia_activacion = time(NULL);
			arr_valores1[0] = "NULL";
			arr_valores1[1] = cast_int_to_string(id);
			arr_valores1[2] = cast_time_to_datetime(fechaocurrencia_activacion);
			arr_valores1[3] = cast_time_to_datetime(time(NULL));
			arr_valores1[4] = "activacion";
			arr_valores1[5] = descripcion;
			res_leertag2 = leertag2 (clsdispositivo, clstag, &valortag );
			if 	((res_leertag2==0) && (valortag.time_fechatiempo > 0)) {
				arr_valores1[6] = cast_double_to_string(valortag.valorfloat); // valortag.valorstr
			}
			else {
				return -1;
			}
			arr_valores1[7] = accionactivar;
			arr_valores1[8] = expresionactivar + " [" + expresionactivar_numerica + "]" ;
			arr_valores1[9] = GLOBAL_nombre_concentrador;
			arr_valores1[10] = clsdispositivo;
			arr_valores1[11] = clstag;
			arr_valores1[12] = "0";
		
		res_insertardblocal = insertar_valor(&stdblocal, "tbhistoalarmas", &arr_campos1[0], &arr_valores1[0], 13);
		
		if (res_insertardblocal == 0) {
			ejecutado_activar_bd = true;
			return 0;
			}
		}
	}
	else	{
		ejecutado_activar_bd = false;
		if 	(ejecutado_desactivar_bd == false) {
			fechaocurrencia_desactivacion = time(NULL);
			arr_valores1[0] = "NULL";
			arr_valores1[1] = cast_int_to_string(id);
			arr_valores1[2] = cast_time_to_datetime(fechaocurrencia_desactivacion);
			arr_valores1[3] = cast_time_to_datetime(time(NULL));
			arr_valores1[4] = "desactivacion";
			arr_valores1[5] = descripcion;
			res_leertag2 = leertag2 (clsdispositivo, clstag, &valortag );
			if 	((res_leertag2==0) && (valortag.time_fechatiempo > 0)) {
				arr_valores1[6] = cast_double_to_string(valortag.valorfloat); // valortag.valorstr
			}
			else {
				return -1;
			}
			arr_valores1[6] = cast_double_to_string(valortag.valorfloat);
			arr_valores1[7] = acciondesactivar;
			arr_valores1[8] = expresiondesactivar + " [" + expresiondesactivar_numerica + "]" ;
			arr_valores1[9] = GLOBAL_nombre_concentrador;
			arr_valores1[10] = clsdispositivo;
			arr_valores1[11] = clstag;
			arr_valores1[12] = "0";
		
		res_insertardblocal = insertar_valor(&stdblocal, "tbhistoalarmas", &arr_campos1[0], &arr_valores1[0], 13);
		
		if (res_insertardblocal == 0) {
			ejecutado_desactivar_bd = true;
			return 0;
			}
		}
	}

return -1;
	
}

