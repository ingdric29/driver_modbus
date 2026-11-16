// Archivo: test_sample_window.cpp
// Descripción: Comprueba última muestra, extremos y deduplicación de ventanas.
// Necesita: Compilación del runner correspondiente y datos aislados de prueba.
// Retornos: 0 al completar las comprobaciones; un error o aserción hace fallar la prueba.

#include "sample_window.h"
#include <cassert>
#include <limits>
int main() {
    SampleWindow window;
    const double voltage[] = {220, 280, 219, 221};
    for (int i = 0; i < 4; ++i) {
        SampleValue sample;
        sample.sequence = i + 1; sample.acquired = 100 + i;
        sample.value = voltage[i]; sample.text = std::to_string(voltage[i]);
        window.observe(sample);
    }
    auto peaks = window.samples(true);
    assert(peaks.size() == 3);
    assert(peaks[0].value == 280 && peaks[0].acquired == 101);
    assert(peaks[1].value == 219);
    assert(peaks[2].value == 221);
    assert(window.samples(false).size() == 1);
    assert(window.samples(false)[0].value == 221);
    window.clear(); assert(window.empty());
    SampleValue single; single.sequence = 5; single.value = 220;
    window.observe(single);
    assert(window.samples(true).size() == 1);
    SampleValue invalid; invalid.sequence = 6; invalid.value = std::numeric_limits<double>::quiet_NaN();
    window.observe(invalid);
    assert(window.samples(true)[0].sequence == 5);
}
