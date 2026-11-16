// Archivo: test_retry.cpp
// Descripción: Comprueba espera progresiva y recuperación de reconexiones.
// Necesita: Compilación del runner correspondiente y datos aislados de prueba.
// Retornos: 0 al completar las comprobaciones; un error o aserción hace fallar la prueba.

#include "retry_backoff.h"
#include <cassert>
int main() {
    RetryBackoff retry;
    auto now = std::chrono::steady_clock::time_point{} + std::chrono::hours(1);
    assert(retry.ready(now)); retry.failed(now);
    assert(!retry.ready(now + std::chrono::milliseconds(99)));
    assert(retry.ready(now + std::chrono::milliseconds(100)));
    retry.failed(now); assert(!retry.ready(now + std::chrono::milliseconds(199)));
    for (int i = 0; i < 30; ++i) retry.failed(now);
    assert(!retry.ready(now + std::chrono::seconds(29)));
    assert(retry.ready(now + std::chrono::seconds(30)));
    retry.succeeded(); assert(retry.ready(now)); retry.failed(now);
    assert(retry.ready(now + std::chrono::milliseconds(100)));
}
