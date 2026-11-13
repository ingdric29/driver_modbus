#ifndef ESTRUCTURAS
	
	#define ESTRUCTURAS
	
	#include "cls_equipos.h"
	#include "string"
	#include "time.h"
	
	struct stres {
	double valorfloat;
	string valorstr;
	string unidad;
	time_t time_fechatiempo;
	int indice;
	cls_equipos* puntero_equipo;
	};
	
	struct stdatabase {
	string direccion;
	int    puerto;
	string nombre;
	string usuario;
	string clave;
};

// estructuras y variables para enviar correos
 
struct upload_status {
    size_t lines_read;
    std::vector<std::string>* payload; 
};
 
struct storigen {
	std::string from;
	std::string username;
	std::string password;
	std::string smtp;
};

struct stdestino {
	std::string to;
	std::string cc;
	std::string asunto;
	std::string cuerpo;
	std::string cuerpo1;
	std::string cuerpo2;
	std::string cuerpo3;
	std::string cuerpo4;
	std::string cuerpo5;
	std::string cuerpo6;
	std::string cuerpo7;
};


struct stdelim {
	std::string tag;
	std::string valor;
	std::string delim1;
	std::string delim2;

};

#endif
