// Modificación hecha por: Ing. Javier Flores 12/11/24

#include "include/cls_email.h" //Ubicacion de la cabecera cambiada
	
	using namespace std;
	
	cls_email::cls_email(){
		
	}
	
	extern struct storigen GLOBAL_email_origen;
	int enviar_email(struct storigen origen  , struct stdestino destino);
	
	int cls_email::enviar() {
		
		int res;
		stdestino LOCAL_destino;
		LOCAL_destino.to = to;
		LOCAL_destino.cc = cc;
		LOCAL_destino.asunto = asunto;
		LOCAL_destino.cuerpo = mensaje;   // primera linea del mensaje
		LOCAL_destino.cuerpo1 = mensaje1; // segunda linea
		LOCAL_destino.cuerpo2 = mensaje2; // tercera linea
		LOCAL_destino.cuerpo3 = mensaje3;
		LOCAL_destino.cuerpo4 = mensaje4;
		LOCAL_destino.cuerpo5 = mensaje5;
		LOCAL_destino.cuerpo6 = mensaje6;
		LOCAL_destino.cuerpo7 = mensaje7;
		
		cout << "to:" << LOCAL_destino.to << endl;
		cout << "cc:" << LOCAL_destino.cc << endl;
		cout << "asunto:" << LOCAL_destino.asunto << endl;
		cout << "cuerpo:" << LOCAL_destino.cuerpo << endl;
				
		res = enviar_email(GLOBAL_email_origen, LOCAL_destino );
		return res;
	}
	
	
