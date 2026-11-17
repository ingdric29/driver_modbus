/*
 * Archivo: s7_batch.h
 * Descripción: Agrupa únicamente direcciones vencidas contiguas o superpuestas.
 * Necesita: Índice de tag/dirección DB y límite útil de bytes de PDU.
 * Retornos: Bloques con índices originales; rechaza rangos inválidos.
 */
#ifndef AUTOMONITOR_S7_BATCH_H
#define AUTOMONITOR_S7_BATCH_H
#include "s7_decode.h"
#include <algorithm>
#include <utility>
#include <vector>
namespace s7 {
struct Batch {
    int db = 0, start = 0, count = 0;
    std::vector<size_t> tags;
};
inline std::vector<Batch> plan(std::vector<std::pair<size_t, Address>> due, int limit) {
    if (limit < 4 || limit > 65536)
        throw std::invalid_argument("S7 invalid batch limit");
    std::sort(due.begin(), due.end(),
              [](const std::pair<size_t, Address> &a, const std::pair<size_t, Address> &b) {
                  return a.second.db != b.second.db ? a.second.db < b.second.db
                                                    : a.second.byte < b.second.byte;
              });
    std::vector<Batch> result;
    for (const auto &item : due) {
        const auto &a = item.second;
        if (a.db < 1 || a.db > 65535 || a.byte < 0 ||
            (a.width != 1 && a.width != 2 && a.width != 4) || a.byte > 2097152 - a.width)
            throw std::invalid_argument("S7 invalid batch address");
        if (!result.empty()) {
            auto &last = result.back();
            int end = std::max(last.start + last.count, a.byte + a.width);
            if (a.db == last.db && a.byte <= last.start + last.count && end - last.start <= limit) {
                last.count = end - last.start;
                last.tags.push_back(item.first);
                continue;
            }
        }
        Batch block;
        block.db = a.db;
        block.start = a.byte;
        block.count = a.width;
        block.tags.push_back(item.first);
        result.push_back(block);
    }
    return result;
}
} // namespace s7
#endif
