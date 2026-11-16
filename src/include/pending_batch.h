/*
 * Archivo: pending_batch.h
 * Descripción: Conserva y reintenta las muestras pendientes de persistencia.
 * Necesita: Lote de muestras y callback de almacenamiento.
 * Retornos: persist devuelve bool e impide reenviar la parte confirmada.
 */

#ifndef DRIVER_PENDING_BATCH_H
#define DRIVER_PENDING_BATCH_H
#include "sample_window.h"
#include <utility>

// One fixed-size window (<=3 samples). Confirm only after a positive database
// acknowledgement; an error never discards the remaining suffix.
class PendingBatch {
    std::vector<SampleValue> samples_;
    size_t confirmed_ = 0;

  public:
    PendingBatch() = default;
    explicit PendingBatch(std::vector<SampleValue> samples) : samples_(std::move(samples)) {}
    bool empty() const { return confirmed_ == samples_.size(); }
    template <class Store> bool persist(Store &&store) {
        std::vector<SampleValue> remaining(samples_.begin() + confirmed_, samples_.end());
        if (remaining.empty())
            return true;
        if (!store(remaining))
            return false;
        confirmed_ = samples_.size();
        return true;
    }
    template <class Store> bool flush(Store &&store) {
        while (!empty()) {
            if (!store(samples_[confirmed_]))
                return false;
            ++confirmed_;
        }
        return true;
    }
};
#endif
