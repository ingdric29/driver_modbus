/*
 * Archivo: equipment_internal.h
 * Descripción: Declara contratos y servicios compartidos entre módulos.
 * Necesita: Definiciones únicas del estado y las implementaciones correspondientes.
 * Retornos: La cabecera no retorna valores por sí sola; los métodos conservan sus contratos
 * documentados en implementación.
 */

#ifndef AUTOMONITOR_EQUIPMENT_INTERNAL_H
#define AUTOMONITOR_EQUIPMENT_INTERNAL_H
#include "cls_equipos.h"
#include "cls_logger.h"
// Shared by protocol implementations. The single instance and all static
// equipment definitions stay in cls_equipos.cpp in their original order.
extern Logger logger;
#endif
