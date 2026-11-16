// Automonitor - separated from main.cpp; public function contracts unchanged.
// Original implementation: Alejandro Diomede / Automatroni.
#include "include/driver_services.h"
#include "include/conversiones.h"
#include "include/runtime_control.h"
#include <algorithm>
#include <cstdlib>
#include <ctime>
#include <iostream>
#include <sstream>

using namespace std;

int cmd_buscarid(int id) {
    if (!cmd)
        return -1;
    int indice = -1;
    stringstream ss1;
    ss1 << "contador comandos:" << GLOBAL_contador_cmd;
    imprimir_mensaje(ss1.str());
    for (int j = 0; j < GLOBAL_contador_cmd; j++) {
        ss1.str("");
        ss1 << "valor id buscado=" << id << " | leido id=" << cmd[j].id << " | " << cmd[j].nombre
            << "indice ram=" << j;
        imprimir_mensaje(ss1.str());
        if (id == cmd[j].id) {
            indice = j;
            break;
        }
    }
    return indice;
}

int email_buscarid(int id) {
    if (!emaillist)
        return -1;
    int indice = -1;
    for (int j = 0; j < GLOBAL_contador_directorio; j++) {
        if (id == emaillist[j].id) {
            indice = j;
            break;
        }
    } //for
    return indice;
}

int sms_buscarid(int id) {
    if (!smslist)
        return -1;
    int indice = -1;
    for (int j = 0; j < GLOBAL_contador_numerossms; j++) {
        if (id == smslist[j].clsid) {
            indice = j;
            break;
        }
    } //for
    return indice;
}
