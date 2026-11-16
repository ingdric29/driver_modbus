// Original implementation: Alejandro Diomede / Automatroni; modularized without logic changes.
#include "include/driver_services.h"
#include "include/cls_defalarmas.h"
#include "include/conversiones.h"
#include <sstream>
#include <vector>

using namespace std;

int cls_defalarmas::ejecutar_accion(const string &accionstr, bool alarmaactivada) {
    std::lock_guard<std::recursive_mutex> commands(GLOBAL_command_mutex);
    size_t pos, pos_ant, inicio, fin;
    std::vector<string> vector_accion;
    int res = 0; // si es 0 todo ok, sino es un error
    int res_cmd = 0;
    int res_email = 0;
    int res_sms = 0;

    pos = 0;
    pos_ant = 0;
    inicio = 0;
    fin = 0;
    int indice = 0;
    string indicestr = "";
    bool ordenencontrada = false;
    vector_accion = split1(accionstr, ';');
    string accion1 = "";
    stringstream ss1;
    stres valorAlarma;
    int res_leertag2;
    try {
        for (std::vector<std::string>::size_type i = 0; i < vector_accion.size(); i++) {
            ordenencontrada = false;
            accion1 = vector_accion[i];
            pos = accion1.find("CMD[", pos_ant);
            if (pos != std::string::npos) { // consiguie la orden cmd
                ordenencontrada = true;
                inicio = pos;
                fin = accion1.find("]", inicio);
                if (fin != std::string::npos) { // busca el cierre del corchete
                    pos = fin;
                    indicestr = accion1.substr(inicio + 4, fin - inicio - 4);
                    ss1 << "comando CMD indice = " << cast_string_to_int(indicestr)
                        << " longitud:" << indicestr.size();
                    imprimir_mensaje(ss1.str());
                    indice = cmd_buscarid(cast_string_to_int(indicestr));
                    ss1.str("");
                    ss1 << "indice en ram comando a ejecutar: " << indice;
                    imprimir_mensaje(ss1.str());
                    if ((indice >= 0) && (indice < GLOBAL_contador_cmd)) {

                        if (alarmaactivada == true) {

                            if (ejecutado_activar_cmd == false) {
                                res_cmd = cmd[indice].ejecutar();
                            }

                        } else {

                            if (ejecutado_desactivar_cmd == false) {
                                res_cmd = cmd[indice].ejecutar();
                            }
                        }
                    }
                } // fin del cierre del corechete

            } // fin de la orden cmd

            if (ordenencontrada == false) {
                pos_ant = 0;
                pos = accion1.find("EMAIL[", pos_ant);
                if (pos != std::string::npos) {
                    ordenencontrada = true;
                    inicio = pos;
                    fin = accion1.find("]", inicio);
                    if (fin != std::string::npos) { // busca el cierre del corchete
                        pos = fin;
                        indicestr = accion1.substr(inicio + 6, fin - inicio - 6);
                        indice = email_buscarid(cast_string_to_int(indicestr));
                        if (emaillist && (indice >= 0) && (indice < GLOBAL_contador_directorio)) {
                            cls_email notification = emaillist[indice];
                            notification.context =
                                "alarma=" + std::to_string(id) + " dispositivo=" + clsdispositivo +
                                " directorio=" + indicestr +
                                (alarmaactivada ? " evento=activar" : " evento=desactivar");
                            //"activar   : " + expresionactivar + "[ " + expresionactivar_numerica + " ]"

                            //"desactivar: " + expresiondesactivar + "[ " + expresiondesactivar_numerica + " ]"

                            if (alarmaactivada == true) {

                                notification.asunto =
                                    "Alarma | " + descripcion + " | " + GLOBAL_nombre_concentrador;

                                notification.mensaje =
                                    "Fecha y hora: " + fecha_para_emails_local_es();
                                notification.mensaje1 =
                                    "Tipo: 	Alarma activada en " + GLOBAL_nombre_concentrador;
                                notification.mensaje2 = descripcion;
                                //notification.mensaje3 = "Dispositivo	: " + clsdispositivo;
                                //notification.mensaje4 = "Direccion ip / origen	: " + clsorigen;
                                //notification.mensaje5 = "Registro				: " + clsregistro;
                                //notification.mensaje6 = "Causa: " + expresionactivar + "[ " + expresionactivar_numerica + " ]";
                                res_leertag2 =
                                    leertag2(clsdispositivo, clsvalorAlarma, &valorAlarma);
                                if ((res_leertag2 == 0) && (valorAlarma.time_fechatiempo > 0)) {
                                    notification.mensaje3 =
                                        cast_double_to_string(valorAlarma.valorfloat, 2);
                                    notification.mensaje4 = valorAlarma.unidad;
                                } else {
                                    notification.mensaje3 = "";
                                    notification.mensaje4 = "";
                                }

                                if (ejecutado_activar_email == false) {
                                    res_email = notification.enviar();
                                }

                            } else {

                                notification.asunto = "Alarma desactivada | " + descripcion +
                                                      " | " + GLOBAL_nombre_concentrador;
                                notification.mensaje =
                                    "Fecha y hora: " + fecha_para_emails_local_es();
                                notification.mensaje1 =
                                    "Tipo: 	Alarma desactivada en " + GLOBAL_nombre_concentrador;
                                notification.mensaje2 = descripcion;
                                //notification.mensaje3 = "Dispositivo	: " + clsdispositivo;
                                //notification.mensaje4 = "Direccion ip / origen	: " + clsorigen;
                                //notification.mensaje5 = "Registro				: " +  clsregistro;

                                //notification.mensaje6 = "Causa: " + expresiondesactivar + "[ " + expresiondesactivar_numerica + " ]";
                                res_leertag2 =
                                    leertag2(clsdispositivo, clsvalorAlarma, &valorAlarma);
                                if ((res_leertag2 == 0) && (valorAlarma.time_fechatiempo > 0)) {
                                    notification.mensaje3 =
                                        cast_double_to_string(valorAlarma.valorfloat, 2);
                                    notification.mensaje4 = valorAlarma.unidad;
                                } else {
                                    notification.mensaje3 = "";
                                    notification.mensaje4 = "";
                                }
                                if (ejecutado_desactivar_email == false) {
                                    res_email = notification.enviar();
                                }
                            }

                        } else {
                            res_email = -1;
                            imprimir_mensaje("No configured EMAIL recipient for ID " + indicestr,
                                             rojo);
                        }

                    } // fin del cierre del corechete
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
                                smslist[indice].clsasunto =
                                    "Alarma Activada [" + cast_time_to_datetime(time(NULL)) +
                                    "] - " + "condicion activar: " + expresionactivar + "[ " +
                                    expresionactivar_numerica + " ]" + " - " + descripcion;
                                if (ejecutado_activar_sms == false) {
                                    res_sms = smslist[indice].enviar();
                                }

                            } else {

                                smslist[indice].clsasunto =
                                    "Alarma Desactivada [" + cast_time_to_datetime(time(NULL)) +
                                    "] - " + "condicion desactivar: " + expresiondesactivar + "[ " +
                                    expresiondesactivar_numerica + " ]" + " - " + descripcion;
                                if (ejecutado_desactivar_sms == false) {
                                    res_sms = smslist[indice].enviar();
                                }
                            }
                        }

                    } // fin del cierre del corechete
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
        } else {
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

        if ((res_cmd == 0) && (res_email == 0) && (res_sms == 0)) {
            res = 0;
        } else {
            res = -1;
        }
        ss1.str("");
        ss1 << "resultado de la accion: " << res << " estado: " << alarmaactivada;
        imprimir_mensaje(ss1.str());

        return res;
    }

    catch (...) {
        imprimir_mensaje(
            "\033[1;31mfalla en el procedimiento cls_defalarmas::ejecutar_accion\033[0m ");
        return -1;
    }
}
