/*
 * Archivo: driver_services.h
 * Descripción: Declara contratos y servicios compartidos entre módulos.
 * Necesita: Definiciones únicas del estado y las implementaciones correspondientes.
 * Retornos: La cabecera no retorna valores por sí sola; los métodos conservan sus contratos
 * documentados en implementación.
 */

#ifndef DRIVER_SERVICES_H
#define DRIVER_SERVICES_H
#include "globales.h"
#include "estructuras.h"
#include "cls_logger.h"
// Shared application services. Workers only retain these while their
// configuration generation is alive; the coordinator owns lifecycle.
extern stdatabase stdblocal;
extern stdatabase stdbexterna;
extern Logger logger_main;
class cls_defalarmas;
class cls_defeventos;
extern cls_defalarmas *defalarmas;
extern cls_defeventos *defeventos;
// Output state remains defined in main.cpp; access is serialized by the
// message helpers in runtime_support.cpp.
extern std::string GLOBAL_ultimomensajemostrado;
extern std::recursive_mutex GLOBAL_output_mutex;
extern int GLOBAL_habilitar_imprimir;
#endif
