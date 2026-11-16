/*
 * Archivo: cls_email.h
 * Descripción: Prepara y envía notificaciones por SMTP mediante libcurl.
 * Necesita: Origen y destino, configuración SMTP autorizada y plantilla HTML.
 * Retornos: Código CURL convertido a int en el envío: 0 para aceptación del transporte, distinto
 * de 0 ante fallo.
 */

#ifndef CLS_EMAIL

#define CLS_EMAIL

#include <string>
#include "estructuras.h"

using namespace std;

class cls_email {
  public:
    //parametros

    int id;
    string context;
    string from;
    string to;
    string cc;
    string asunto;
    string mensaje;
    string mensaje1;
    string mensaje2;
    string mensaje3;
    string mensaje4;
    string mensaje5;
    string mensaje6;
    string mensaje7;

    cls_email();
    int enviar();
};

#endif
