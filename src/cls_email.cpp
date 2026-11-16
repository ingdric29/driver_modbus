/*
 * Archivo: cls_email.cpp
 * Descripción: Prepara y envía notificaciones por SMTP mediante libcurl.
 * Necesita: Origen y destino, configuración SMTP autorizada y plantilla HTML.
 * Retornos: Código CURL convertido a int en el envío: 0 para aceptación del transporte, distinto
 * de 0 ante fallo.
 */

#include "include/cls_email.h" //Ubicacion de la cabecera cambiada

using namespace std;

/**
 * Inicializa el identificador del destinatario a cero.
 * Parámetros: ninguno; utiliza el contexto descrito en el encabezado.
 * Retorno: Sin valor de retorno (constructor).
 */
cls_email::cls_email() : id(0) {}

extern struct storigen GLOBAL_email_origen;
int enviar_email(struct storigen origen, struct stdestino destino);

/**
 * Prepara el destino y utiliza el origen SMTP global.
 * Parámetros: ninguno; utiliza el contexto descrito en el encabezado.
 * Retorno: Código CURL: 0 si el transporte acepta el mensaje; distinto de 0 ante fallo.
 */
int cls_email::enviar() {

    int res;
    stdestino LOCAL_destino;
    LOCAL_destino.context = context.empty() ? "directorio=" + std::to_string(id) : context;
    LOCAL_destino.to = to;
    LOCAL_destino.cc = cc;
    LOCAL_destino.asunto = asunto;
    LOCAL_destino.cuerpo = mensaje;   // primera linea del mensaje
    LOCAL_destino.cuerpo1 = mensaje1; // segunda linea
    LOCAL_destino.cuerpo2 = mensaje2; // tercera linea
    LOCAL_destino.cuerpo3 = mensaje3;
    LOCAL_destino.cuerpo4 = mensaje4;
    LOCAL_destino.cuerpo5 = mensaje5;
    LOCAL_destino.cuerpo6 = mensaje6;
    LOCAL_destino.cuerpo7 = mensaje7;

    res = enviar_email(GLOBAL_email_origen, LOCAL_destino);
    return res;
}
