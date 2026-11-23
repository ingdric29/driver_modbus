/*
 * Archivo: command_repository.h
 * Descripción: Carga listas privadas de ejecución sin sustituir acciones de alarmas.
 * Necesita: Perfil SQL, concentrador global y lote solicitado.
 * Retornos: Cantidad cargada, 0 vacío/omitido, -1 error; nunca entrega resultados parciales.
 */
#ifndef DRIVER_COMMAND_REPOSITORY_H
#define DRIVER_COMMAND_REPOSITORY_H
#include "cls_cmd.h"
#include <vector>

namespace command_repository {
constexpr size_t maximum_commands = 4096;
int load_batch(const std::string &host, int port, const std::string &schema,
               const std::string &user, const std::string &password, const std::string &batch,
               std::vector<cls_cmd> &commands);
} // namespace command_repository
#endif
