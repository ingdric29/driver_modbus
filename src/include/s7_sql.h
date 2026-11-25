/* Catálogo y persistencia Siemens: requiere perfil SQL existente y muestras válidas.
 * No modifica esquemas. Las llamadas de almacenamiento pertenecen al escritor. */
#ifndef AUTOMONITOR_S7_SQL_H
#define AUTOMONITOR_S7_SQL_H
#include "local_acquisition.h"
#include "sample_window.h"
#include "s7_runtime_samples.h"
#include <mutex>
class WorkerGroup;
namespace local_driver {
using SqlSample = s7_runtime::Sample;
struct SqlTag {
    int device_id = 0, register_id = 0;
    std::string device, concentrator, trace;
    Tag tag;
    bool registrable = true, only_changes = false;
    std::shared_ptr<SqlSample> sample = std::make_shared<SqlSample>();
};
struct SqlCatalog {
    Config config;
    std::vector<SqlTag> tags;
};
SqlCatalog load_sql_s7();
enum class StoreResult { Stored, Retry, Uncertain };
StoreResult store_s7(const SqlTag &tag, const SampleValue &sample, bool instant, bool history);
void write_sql_s7(std::vector<SqlTag> tags);
void start_sql_s7(WorkerGroup &workers);
} // namespace local_driver
#endif
