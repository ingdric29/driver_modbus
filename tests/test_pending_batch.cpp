// Archivo: test_pending_batch.cpp
// Descripción: Comprueba confirmación parcial y reintento de muestras pendientes.
// Necesita: Compilación del runner correspondiente y datos aislados de prueba.
// Retornos: 0 al completar las comprobaciones; un error o aserción hace fallar la prueba.

#include "pending_batch.h"
#include <cassert>
int main() {
    SampleValue one, two, three;
    one.sequence = 1; two.sequence = 2; three.sequence = 3;
    PendingBatch batch({one, two, three});
    std::vector<uint64_t> calls;
    assert(!batch.flush([&](const SampleValue& s) { calls.push_back(s.sequence); return s.sequence != 2; }));
    assert((calls == std::vector<uint64_t>{1, 2}));
    assert(!batch.empty());
    calls.clear();
    assert(batch.flush([&](const SampleValue& s) { calls.push_back(s.sequence); return true; }));
    assert((calls == std::vector<uint64_t>{2, 3}));
    assert(batch.empty());
    assert(batch.flush([](const SampleValue&) { assert(false); return false; }));
}
