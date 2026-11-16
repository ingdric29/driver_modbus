// Archivo: test_workers.cpp
// Descripción: Comprueba arranque, unión y propagación de errores de trabajadores.
// Necesita: Compilación del runner correspondiente y datos aislados de prueba.
// Retornos: 0 al completar las comprobaciones; un error o aserción hace fallar la prueba.

#include "worker_group.h"
#include <cassert>
#include <stdexcept>
int main() {
    std::atomic<bool> running{true};
    std::atomic<int> active{0};
    // Covers normal stop/join and stack unwinding during partial startup.
    for (int generation = 0; generation < 10; ++generation) {
        running = true;
        try {
            WorkerGroup workers(running);
            for (int i = 0; i < 4; ++i) workers.start([&] {
                ++active;
                while (running) std::this_thread::yield();
                --active;
            });
            if (generation % 2) throw std::runtime_error("partial startup");
            running = false;
            workers.join();
        } catch (const std::runtime_error&) {}
        assert(active == 0);
        assert(!running);
    }
    bool propagated = false; running = true;
    try {
        WorkerGroup workers(running);
        workers.start([] { throw std::runtime_error("worker failed"); });
        workers.start([&] { while (running) std::this_thread::yield(); });
        workers.join();
    } catch (const std::runtime_error&) { propagated = true; }
    assert(propagated && !running);

}
