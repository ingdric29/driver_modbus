#ifndef CLS_DEFALARMAS
	#define CLS_DEFALARMAS
	
	#include "cls_equipos.h"
	#include "cls_cmd.h"

	#include "globales.h"
	#include <time.h>
	#include <string>

	
	using namespace std;
	
struct stres;

class cls_defalarmas {
		public:
		int id;
		string clsdispositivo;
		string clstag;
		string clsvalorAlarma;
		string clsorigen;
		string clsregistro;
		string expresionactivar;
		string expresiondesactivar;
		string expresionactivar_numerica;
		string expresiondesactivar_numerica;
		string descripcion;
		string accionactivar;
		string acciondesactivar;
		bool habilitar;
		bool activa;
		bool reconocida;
		bool ejecutado_activar_cmd;
		bool ejecutado_desactivar_cmd;
		bool ejecutado_activar_email;
		bool ejecutado_desactivar_email;
		bool ejecutado_activar_bd;
		bool ejecutado_desactivar_bd;
		bool ejecutado_activar_sms;
		bool ejecutado_desactivar_sms;
		bool ejecutado_activar;
		bool ejecutado_desactivar;
		int  contador_de_acciones_activar; // cuenta el numero de acciones realizadas
		int  contador_de_acciones_desactivar; // cuenta el numero de acciones realizadas
		int  contador_de_acciones_max; //maximo numero de acciones realizadas
		cls_equipos* clsequipo_mbtcp;
		int 		 clscantidad_equipo_mbtcp;
		cls_equipos* clsequipo_mbserial;
		int 		 clscantidad_equipo_mbserial;
		cls_equipos* clsequipo_rs232;
		int 		 clscantidad_equipo_rs232;
		cls_cmd* 	 clscmd;
		int 		 clscantidad_cmd;
		cls_email* 	 clsemaillist;
		int 		 clscantidademaillist;
		time_t ultima_evaluacion;
		time_t fechaocurrencia_activacion;
		time_t fechaocurrencia_desactivacion;
		// metodos
		int evaluarcompleto();
		cls_defalarmas();
		
		
		private:
		string reemplazar_simbolos(const string& cadena1);
		int leertag2 (const string& dispositivo, const string& tag, struct stres * local );
		string reordenar_antes_evaluar(string in);
		int evaluar1(string p);
		string eliminar_espacios(string p);
		int ejecutar_accion(const string& accionstr, bool alarmaactivada);
		int insertar_tbhistoalarmas();

	};
	
#endif	


