#ifndef CLS_DEFEVENTOS
#define CLS_DEFEVENTOS

#include "cls_equipos.h"
#include "cls_cmd.h"

#include "globales.h"
#include <time.h>
#include <string>

using namespace std;

struct stres;

class cls_defeventos {
  public:
    int id;
    bool habilitar;
    string nombredispositivo;
    string tag;
    int bit1;
    string activado;
    string desactivado;
    string comentario;
    int indiceenram;
    string protocolo;

    int valoranterior;
    int bitvaloranterior;
    time_t tiempoanterior;
    int valoractual;
    int bitvaloractual;
    time_t tiempoactual;
    int *pvaloractual;
    cls_equipos *pequipo; //puntero a equipo

    // metodos
    cls_defeventos();
    int cargar_en_memoriaram(void);
    int cargar_posicion(void);
    int leer_valorenram(void);
};

#endif
