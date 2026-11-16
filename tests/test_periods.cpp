// Archivo: test_periods.cpp
// Descripción: Comprueba plazos monotónicos y períodos de adquisición/persistencia.
// Necesita: Compilación del runner correspondiente y datos aislados de prueba.
// Retornos: 0 al completar las comprobaciones; un error o aserción hace fallar la prueba.

#include "periodic_deadline.h"
#include <cassert>
#include <limits>
int main() {
    using D = PeriodicDeadline;
    const auto origin = D::Time{} + std::chrono::hours(1);
    D voltage, temperature, history;
    int voltage_reads = 0, temperature_reads = 0, writes = 0;
    for (int milliseconds = 0; milliseconds <= 10000; milliseconds += 10) {
        auto now = origin + std::chrono::milliseconds(milliseconds);
        if (voltage.due(now)) { ++voltage_reads; voltage.advance(0.1, now); }
        if (temperature.due(now)) { ++temperature_reads; temperature.advance(2, now); }
        if (history.due(now)) { ++writes; history.advance(5, now); }
    }
    assert(voltage_reads == 101);
    assert(temperature_reads == 6);
    assert(writes == 3);
    // A delayed request skips missed slots rather than issuing a burst.
    voltage.advance(0.1, origin + std::chrono::seconds(25));
    assert(voltage.next() == origin + std::chrono::milliseconds(25100));
    for (double bad : {0., -1., std::numeric_limits<double>::infinity(), std::numeric_limits<double>::quiet_NaN()}) {
        bool rejected = false;
        try { D::interval(bad); } catch (const std::invalid_argument&) { rejected = true; }
        assert(rejected);
    }
}
