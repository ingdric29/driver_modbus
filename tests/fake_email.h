// Archivo: fake_email.h
// Descripción: Doble de transporte de correo para comprobar alarmas sin envíos externos.
// Necesita: Compilación del runner correspondiente y datos aislados de prueba.
// Retornos: Las aserciones o errores hacen fallar la prueba; las bibliotecas de fixtures aportan datos simulados.

#ifndef DRIVER_TEST_FAKE_EMAIL_H
#define DRIVER_TEST_FAKE_EMAIL_H
#include "estructuras.h"
#include <vector>
namespace test_email {
extern std::vector<stdestino> deliveries;
extern int result;
}
#endif
