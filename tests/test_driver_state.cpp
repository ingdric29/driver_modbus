#include "globales.h"
#include <cassert>
#include <cstring>
int main() {
    cls_equipos tag;
    tag.protocolo = "RS232";
    tag.nombre = "serial-test";
    tag.tag = "temperature";
    tag.html_id = "<v>";
    tag.html_final = "</v>";
    const char* frame = "<v>12.5</v>";
    std::memcpy(tag.buf, frame, std::strlen(frame));
    tag.n_buf = std::strlen(frame);
    assert(tag.leer() == 1);
    assert(tag.resultado_leer);
    assert(tag.clsvalorfloat == 12.5f);
    std::atomic<bool> running{true};
    std::thread producer([&] {
        for (int i = 0; i < 1000; ++i) {
            {
                std::lock_guard<std::mutex> lock(*tag.buffer_mutex);
                const char* next = i % 2 ? "<v>15.0</v>" : "<v>20.0</v>";
                std::memcpy(tag.buf, next, std::strlen(next));
                tag.n_buf = std::strlen(next);
            }
            tag.leer();
        }
        running = false;
    });
    while (running) {
        std::lock_guard<std::recursive_mutex> lock(*tag.state_mutex);
        assert(tag.clsvalorfloat == 12.5f || tag.clsvalorfloat == 15 || tag.clsvalorfloat == 20);
        assert(tag.clsvalor == static_cast<int>(tag.clsvalorfloat));
    }
    producer.join();
    tag.n_buf = 0;
    assert(tag.leer() == -1);
    assert(!tag.resultado_leer);
}
