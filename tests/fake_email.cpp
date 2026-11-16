// Archivo: fake_email.cpp
// Descripción: Doble de transporte de correo para comprobar alarmas sin envíos externos.
// Necesita: Compilación del runner correspondiente y datos aislados de prueba.
// Retornos: Las aserciones o errores hacen fallar la prueba; las bibliotecas de fixtures aportan datos simulados.

#include "fake_email.h"
namespace test_email {
std::vector<stdestino> deliveries;
int result = 0;
}
// Tests replace the transport at link time. No SMTP connection is made.
int enviar_email(storigen, stdestino destination) {
    test_email::deliveries.push_back(destination);
    return test_email::result;
}
