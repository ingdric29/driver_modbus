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
#endif
