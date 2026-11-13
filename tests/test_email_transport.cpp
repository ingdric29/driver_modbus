#include "estructuras.h"
#include <curl/curl.h>
#include <iostream>
#include <string>
std::string GLOBAL_nombre_concentrador = "SMTP fixture";
int enviar_email(storigen, stdestino);
int main(int argc, char** argv) {
    if (argc != 3) return 2;
    if (curl_global_init(CURL_GLOBAL_DEFAULT) != CURLE_OK) return 3;
    storigen origin;
    origin.smtp = argv[1]; origin.ca_file = std::string(argv[2]) == "-" ? "" : argv[2];
    origin.from = "sender@example.invalid";
    origin.username = "fixture-user"; origin.password = "fixture-secret";
    stdestino destination;
    destination.context = "alarma=42 dispositivo=meter directorio=7 evento=activar";
    destination.to = "recipient@example.invalid";
    destination.asunto = "SMTP transport fixture";
    destination.cuerpo2 = std::string(120000, 'X') + "END_LARGE_DESCRIPTION";
    destination.cuerpo3 = "280"; destination.cuerpo4 = "V";
    const int result = enviar_email(origin, destination);
    std::cout << "RESULT=" << result << std::endl;
    curl_global_cleanup();
    return result;
}
