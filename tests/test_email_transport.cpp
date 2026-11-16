// Archivo: test_email_transport.cpp
// Descripción: Comprueba transporte SMTP real contra servidores de prueba locales.
// Necesita: Compilación del runner correspondiente y datos aislados de prueba.
// Retornos: 0 al completar las comprobaciones; un error o aserción hace fallar la prueba.

#include "estructuras.h"
#include "globales.h"
#include <curl/curl.h>
#include <iostream>
#include <string>
std::string GLOBAL_nombre_concentrador = "SMTP fixture";
// Freeze only the clock used in message text for the original/current comparison.
std::string fecha_para_emails_local() { return "Vie, 13 Nov 2026 13:00:00"; }
void imprimir_mensaje(std::string, GLOBAL_colores) {}
int enviar_email(storigen, stdestino);
int main(int argc, char** argv) {
    if (argc != 3) return 2;
    if (curl_global_init(CURL_GLOBAL_DEFAULT) != CURLE_OK) return 3;
    storigen origin;
    origin.smtp = argv[1];
    origin.from = "sender@example.invalid";
    origin.username = "fixture-user"; origin.password = "fixture-secret";
    stdestino destination;
    destination.context = "alarma=42 dispositivo=meter directorio=7 evento=activar";
    destination.to = "recipient@example.invalid";
    destination.cc = std::string(argv[2]) == "cc" ? "carbon@example.invalid" : "";
    destination.asunto = "SMTP transport fixture";
    destination.cuerpo2 = std::string(argv[2]) == "large"
        ? std::string(120000, 'X') + "END_LARGE_DESCRIPTION" : "voltage spike";
    destination.cuerpo3 = "280"; destination.cuerpo4 = "V";
    const int result = enviar_email(origin, destination);
    std::cout << "RESULT=" << result << std::endl;
    curl_global_cleanup();
    return result;
}
