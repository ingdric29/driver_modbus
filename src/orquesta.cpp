/*
    Ultima modificacion: 19/11/2024
    Hecha por: Ing. Javier Flores

    + Se añadieron 4 librerias nuevas: cls_logger, boost::filesystem, cstdlib y unistd.
    + El programa busca automaticamente donde se encuentra el ejecutable del automonitor
*/


#include <stdlib.h>
#include <stdio.h>
#include <iostream>
#include <time.h>
#include <inttypes.h>
#include <math.h>
#include <string.h>
#include <inttypes.h>
#include "include/cls_logger.h"
#include <boost/filesystem.hpp> // Biblioteca Boost.Filesystem
#include <cstdlib>              // Para std::system
#include <unistd.h>             // Para chdir (cambiar de directorio)

Logger logger_orquesta;

void esperar (double);

using namespace std;



namespace fs = boost::filesystem;

int main(int argc, char* argv[]) {
    try {
        // Obtén la ruta del ejecutable de Orquesta
        fs::path ruta_orquesta = fs::canonical(argv[0]);
        fs::path carpeta_base = ruta_orquesta.parent_path();

        // Verifica si el directorio donde está automonitorv1 existe
        if (!fs::exists(carpeta_base) || !fs::is_directory(carpeta_base)) {
            std::cerr << "Error: No se pudo encontrar el directorio base: " << carpeta_base << std::endl;
            return 1;
        }

        // Cambiar al directorio donde está automonitorv1
        if (chdir(carpeta_base.string().c_str()) != 0) {
            std::cerr << "Error: No se pudo cambiar al directorio: " << carpeta_base << std::endl;
            return 1;
        }

        while (true) {
            if (argc == 1) {
                std::cout << "Iniciando automonitorv1 en el directorio: " << carpeta_base << std::endl;

                // Ejecuta Automonitorv1 desde el directorio actual
                int resultado = std::system("./automonitorv1");

                if (resultado != 0) {
                    std::cerr << "Error: automonitorv1 terminó con código de salida: " << resultado << std::endl;
                }

                // Simula un registro (reemplázalo con tu logger real)
                std::cout << "INFO: Automonitorv1 ejecutado desde el directorio actual." << std::endl;
            }
			
			cout << "Reiniciando el sistema en 60 seg" << endl;
			logger_orquesta.registrarError("INFO: Restarting automonitor in 60 seconds");
			esperar(60.0);	
		}
	   
    } catch (const fs::filesystem_error& e) {
        std::cerr << "Error de Boost.Filesystem: " << e.what() << std::endl;
        return 1;
    }

    return 0;
}




/*
int main(int argc, char * argv[]) { 

	char result[300];
	logger_orquesta.registrarError("INFO: Start Orquesta");
	while(true) {
		if (argc == 1) {
		   cout << "iniciando automonitorv1 de manera predeterminada!" << endl;
		   system("./automonitorv1");
		   logger_orquesta.registrarError("INFO: Initializing automonitorv1 with DEFAULT settings.");

	  }
		else if (argc == 3) {
				strcpy(result,argv[1]);
				strcat(result,"/");
				strcat(result,argv[2]);
				cout << "iniciando automonitor con los argumentos indicados" << endl;
				logger_orquesta.registrarError("INFO: Initializing automonitor with the SPECIFIED parameters.");
				system(result);		  
		  
	  }		
	  //

	  cout << "Reiniciando el sistema en 60 seg" << endl;
	  logger_orquesta.registrarError("INFO: Restarting automonitor in 60 seconds");
	  esperar(60.0);
	}
	
	return 0;
	
}*/

void esperar(double segundos){
time_t comienzo, final1;
double diferencia=0.0;
//struct tm * pstcomienzo;
comienzo = time(NULL);
//pstcomienzo = gmtime(&comienzo);
while(diferencia < segundos) {
    final1 = time(NULL);
    diferencia = difftime(final1,comienzo);
    
}

}