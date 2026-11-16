/*
 * Archivo: sample_window.h
 * Descripción: Conserva muestras mínima, máxima y última de una ventana.
 * Necesita: Muestras válidas con secuencia y fecha de adquisición.
 * Retornos: Lista de muestras deduplicada o bool de ventana vacía; observe/clear son void.
 */

#ifndef DRIVER_SAMPLE_WINDOW_H
#define DRIVER_SAMPLE_WINDOW_H
#include <cstdint>
#include <ctime>
#include <string>
#include <vector>
#include <algorithm>
#include <cmath>

struct SampleValue {
    uint64_t sequence = 0;
    time_t acquired = 0;
    double value = 0;
    std::string text;
};
// Bounded memory: retain actual acquired samples, not synthetic extrema.
class SampleWindow {
    bool populated_ = false;
    SampleValue first_, minimum_, maximum_, last_;

  public:
    void observe(const SampleValue &sample) {
        if (!std::isfinite(sample.value))
            return;
        if (!populated_) {
            first_ = minimum_ = maximum_ = sample;
            populated_ = true;
        }
        if (sample.value < minimum_.value)
            minimum_ = sample;
        if (sample.value > maximum_.value)
            maximum_ = sample;
        last_ = sample;
    }
    bool empty() const { return !populated_; }
    std::vector<SampleValue> samples(bool preserve_peaks) const {
        if (!populated_)
            return {};
        std::vector<SampleValue> values = preserve_peaks
                                              ? std::vector<SampleValue>{minimum_, maximum_, last_}
                                              : std::vector<SampleValue>{last_};
        std::sort(values.begin(), values.end(), [](const SampleValue &a, const SampleValue &b) {
            return a.sequence < b.sequence;
        });
        values.erase(std::unique(values.begin(), values.end(),
                                 [](const SampleValue &a, const SampleValue &b) {
                                     return a.sequence == b.sequence;
                                 }),
                     values.end());
        return values;
    }
    void clear() { populated_ = false; }
};
#endif
