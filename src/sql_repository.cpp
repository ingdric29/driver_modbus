/*
 * Archivo: sql_repository.cpp
 * Descripción: Inserta, busca y actualiza registros con parámetros SQL.
 * Necesita: Perfil de base, tablas, campos, valores y claves indicados en cada llamada.
 * Retornos: Búsqueda: 1/0/-1; escritura: contrato por función. No equivale a confirmación física
 * Modbus.
 */

#include "include/driver_services.h"
#include "include/conversiones.h"
#include "include/repository_support.h"
#include "include/sql_session.h"
#include <cppconn/exception.h>
#include <cppconn/statement.h>
#include <cppconn/prepared_statement.h>
#include <cppconn/resultset.h>

using namespace std;

using repository_detail::database_allowed;
using repository_detail::database_failure;
using repository_detail::database_session;

namespace {
/**
 * Ejecuta una escritura SQL parametrizada con los campos indicados.
 * Parámetros: stdatabase *db, const std::string &table, string *fields, string *values, int
 * count, const std::vector<int> &keys, const std::string &condition = ""
 * Retorno: 0 si completa la operación; -1 por validación, acceso o fallo SQL.
 */
int update_fields(stdatabase *db, const std::string &table, string *fields, string *values,
                  int count, const std::vector<int> &keys, const std::string &condition = "") {
    if (!database_allowed(db) || count <= 0)
        return -1;
    try {
        std::string query = "UPDATE " + sql_identifier(table) + " SET ";
        for (int i = 0; i < count; ++i)
            query += (i ? "," : "") + sql_identifier(fields[i]) + "=?";
        query += " WHERE ";
        if (!condition.empty())
            query += condition; // Internal SQL expression, never a bound value.
        else
            for (size_t i = 0; i < keys.size(); ++i) {
                if (keys[i] < 0 || keys[i] >= count)
                    return -1;
                query += (i ? " AND " : "") + sql_identifier(fields[keys[i]]) + "=?";
            }
        auto connection = database_session(db);
        SqlObservation observation(connection.get());
        std::unique_ptr<sql::PreparedStatement> statement(connection->prepareStatement(query));
        int parameter = 1;
        for (int i = 0; i < count; ++i)
            bind_legacy(*statement, parameter++, values[i]);
        for (int key : keys)
            bind_legacy(*statement, parameter++, values[key]);
        statement->executeUpdate();
        observation.success();
        return 0;
    } catch (const std::exception &error) {
        database_failure(error);
        return -1;
    }
}

/**
 * Busca una coincidencia de campos mediante parámetros SQL.
 * Parámetros: stdatabase *db, const std::string &table, const std::vector<std::string> &fields,
 * const std::vector<std::string> &values
 * Retorno: 1 si existe; 0 si no existe; -1 por acceso o error SQL.
 */
int find_fields(stdatabase *db, const std::string &table, const std::vector<std::string> &fields,
                const std::vector<std::string> &values) {
    if (!database_allowed(db))
        return -1;
    try {
        std::string query = "SELECT 1 FROM " + sql_identifier(table) + " WHERE ";
        for (size_t i = 0; i < fields.size(); ++i)
            query += (i ? " AND " : "") + sql_identifier(fields[i]) + "=?";
        query += " LIMIT 1";
        auto connection = database_session(db);
        SqlObservation observation(connection.get());
        std::unique_ptr<sql::PreparedStatement> statement(connection->prepareStatement(query));
        for (size_t i = 0; i < values.size(); ++i)
            statement->setString(i + 1, values[i]);
        std::unique_ptr<sql::ResultSet> result(statement->executeQuery());
        int found = result->next() ? 1 : 0;
        observation.success();
        return found;
    } catch (const std::exception &error) {
        database_failure(error);
        return -1;
    }
}
} // namespace

/**
 * Guarda una muestra histórica mediante transacción.
 * Parámetros: time_t fechatiempo, string tag, string valorstr, float valorfloat, int
 * dispositivo, string rastro, string nombre_concentrador, string nombre_dispositivo, string
 * direccion_db, int puerto_db, string nombre_db, string usuario_db, string clave_db, string
 * dbtabla
 * Retorno: 1 si confirma la muestra; -1 ante fallo.
 */
int insertardb(time_t fechatiempo, string tag, string valorstr, float valorfloat, int dispositivo,
               string rastro, string nombre_concentrador, string nombre_dispositivo,
               string direccion_db, int puerto_db, string nombre_db, string usuario_db,
               string clave_db, string dbtabla) {
    try {
        SampleValue sample;
        sample.acquired = fechatiempo;
        sample.text = valorstr;
        sample.value = valorfloat;
        auto connection = sql_connection("tcp://" + direccion_db + ":" + std::to_string(puerto_db),
                                         usuario_db, clave_db, nombre_db);
        store_sample_batch(connection, dbtabla, {sample}, tag, dispositivo, rastro,
                           nombre_concentrador, nombre_dispositivo);
        return 1;
    } catch (const std::exception &error) {
        database_failure(error);
        return -1;
    }
}

/**
 * Busca una coincidencia de campos mediante parámetros SQL.
 * Parámetros: stdatabase *db, string tabla, string campo, string valor
 * Retorno: 1 si existe; 0 si no existe; -1 por acceso o error SQL.
 */
int buscarentabla(stdatabase *db, string tabla, string campo, string valor) {
    return find_fields(db, tabla, {campo}, {valor});
}

/**
 * Busca una coincidencia de campos mediante parámetros SQL.
 * Parámetros: stdatabase *db, string tabla, string campo1, string campo2, string campo3, string
 * valor1, string valor2, string valor3
 * Retorno: 1 si existe; 0 si no existe; -1 por acceso o error SQL.
 */
int buscarentabla3(stdatabase *db, string tabla, string campo1, string campo2, string campo3,
                   string valor1, string valor2, string valor3) {
    return find_fields(db, tabla, {campo1, campo2, campo3}, {valor1, valor2, valor3});
}

/**
 * Ejecuta una escritura SQL parametrizada con los campos indicados.
 * Parámetros: stdatabase *db, string tabla, int indice, string *campo, string *valor, int
 * cantidad
 * Retorno: 0 si completa la operación; -1 por validación, acceso o fallo SQL.
 */
int actualizar_valor(stdatabase *db, string tabla, int indice, string *campo, string *valor,
                     int cantidad) {
    return update_fields(db, tabla, campo, valor, cantidad, {indice});
}

/**
 * Ejecuta una escritura SQL parametrizada con los campos indicados.
 * Parámetros: stdatabase *db, string tabla, int indice1, int indice2, int indice3, string
 * *campo, string *valor, int cantidad
 * Retorno: 0 si completa la operación; -1 por validación, acceso o fallo SQL.
 */
int actualizar_valor3(stdatabase *db, string tabla, int indice1, int indice2, int indice3,
                      string *campo, string *valor, int cantidad) {
    return update_fields(db, tabla, campo, valor, cantidad, {indice1, indice2, indice3});
}

/**
 * Ejecuta una escritura SQL parametrizada con los campos indicados.
 * Parámetros: stdatabase *db, string tabla, string campo, string valor, string condicion
 * Retorno: 0 si completa la operación; -1 por validación, acceso o fallo SQL.
 */
int actualizar_valor_condicion(stdatabase *db, string tabla, string campo, string valor,
                               string condicion) {
    return update_fields(db, tabla, &campo, &valor, 1, {}, condicion);
}

/**
 * Ejecuta una escritura SQL parametrizada con los campos indicados.
 * Parámetros: stdatabase *db, string tabla, string *campo, string *valor, int cantidad
 * Retorno: 0 si completa la operación; -1 por validación, acceso o fallo SQL.
 */
int insertar_valor(stdatabase *db, string tabla, string *campo, string *valor, int cantidad) {
    if (!database_allowed(db) || cantidad <= 0)
        return -1;
    try {
        std::string query = "INSERT INTO " + sql_identifier(tabla) + " (", placeholders;
        for (int i = 0; i < cantidad; ++i) {
            query += (i ? "," : "") + sql_identifier(campo[i]);
            placeholders += i ? ",?" : "?";
        }
        query += ") VALUES (" + placeholders + ")";
        auto connection = database_session(db);
        SqlObservation observation(connection.get());
        std::unique_ptr<sql::PreparedStatement> statement(connection->prepareStatement(query));
        for (int i = 0; i < cantidad; ++i)
            bind_legacy(*statement, i + 1, valor[i]);
        int affected = statement->executeUpdate();
        observation.success();
        return affected == 1 ? 0 : -1;
    } catch (const std::exception &error) {
        database_failure(error);
        return -1;
    }
}

/**
 * Actualiza si existe la clave o inserta un nuevo registro.
 * Parámetros: struct stdatabase *db, string tabla, int indice, string *campo, string *valor, int
 * cantidad
 * Retorno: 0 si la escritura es satisfactoria; -1 ante fallo.
 */
int insertar_inteligente(struct stdatabase *db, string tabla, int indice, string *campo,
                         string *valor, int cantidad) {
    int res = -1;
    int rescmd = -1;
    int resfinal = -1;
    res = -1;
    rescmd = -1;
    res = buscarentabla(db, tabla, campo[indice], valor[indice]);
    if (res < 0)
        return -1;
    if (res > 0) {
        //actualiza
        rescmd = actualizar_valor(db, tabla, indice, &campo[0], &valor[0], cantidad);
    } else {
        //inserta
        rescmd = insertar_valor(db, tabla, &campo[0], &valor[0], cantidad);
    }
    if (rescmd == 0) {
        resfinal = 0;
    }

    return resfinal;
}

/**
 * Actualiza si existe la clave o inserta un nuevo registro.
 * Parámetros: struct stdatabase *db, string tabla, int indice1, int indice2, int indice3, string
 * *campo, string *valor, int cantidad
 * Retorno: 0 si la escritura es satisfactoria; -1 ante fallo.
 */
int insertar_inteligente3(struct stdatabase *db, string tabla, int indice1, int indice2,
                          int indice3, string *campo, string *valor, int cantidad) {
    int res = -1;
    int rescmd = -1;
    int resfinal = -1;
    res = -1;
    rescmd = -1;
    res = buscarentabla3(db, tabla, campo[indice1], campo[indice2], campo[indice3], valor[indice1],
                         valor[indice2], valor[indice3]);
    if (res < 0)
        return -1;
    if (res > 0) {
        //actualiza
        rescmd =
            actualizar_valor3(db, tabla, indice1, indice2, indice3, &campo[0], &valor[0], cantidad);
    } else {
        //inserta
        rescmd = insertar_valor(db, tabla, &campo[0], &valor[0], cantidad);
    }
    if (rescmd == 0) {
        resfinal = 0;
    }

    return resfinal;
}
