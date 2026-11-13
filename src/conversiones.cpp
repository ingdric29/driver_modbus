/*
	Modificado por: Ing. Javier Flores
	Ultima modificacion: 12/11/2024
*/

#include "include/conversiones.h"
#include <string.h>
#include <stdexcept>
#include <iomanip>
#include <locale>

using namespace std;

/* Get a float from 4 bytes (Modbus) without any conversion (ABCD) */
float cast_get_float_abcd(const uint16_t* src)
{
	float f;
	uint32_t i;

	i = ntohl(((uint32_t)src[0] << 16) + src[1]);
	memcpy(&f, &i, sizeof(float));

	return f;
}

/* Get a float from 4 bytes (Modbus) in inversed format (DCBA) */
float cast_get_float_dcba(const uint16_t* src)
{
	float f;
	uint32_t i;
	// windows  _byteswap_ulong()
	// linux bswap_32()

#ifdef _WIN32
	i = ntohl(_byteswap_ulong((((uint32_t)src[0]) << 16) + src[1]));
#else
	i = ntohl(bswap_32((((uint32_t)src[0]) << 16) + src[1]));
#endif

	memcpy(&f, &i, sizeof(float));
	return f;
}

/* Get a float from 4 bytes (Modbus) with swapped bytes (BADC) */
float cast_get_float_badc(const uint16_t* src)
{
	float f;
	uint32_t i;
	// windows _byteswap_ushort
	// linux bswap_16

#ifdef _WIN32
	i = ntohl((uint32_t)(_byteswap_ushort(src[0]) << 16) + _byteswap_ushort(src[1]));
#else
	i = ntohl((uint32_t)(bswap_16(src[0]) << 16) + bswap_16(src[1]));
#endif
	
	memcpy(&f, &i, sizeof(float));

	return f;
}

/* Get a float from 4 bytes (Modbus) with swapped words (CDAB) */
float cast_get_float_cdab(const uint16_t* src)
{
	float f;
	uint32_t i;

	i = ntohl((((uint32_t)src[1]) << 16) + src[0]);
	memcpy(&f, &i, sizeof(float));

	return f;
}


time_t cast_datetime_to_time(const string& value) {
    std::tm parsed{};
    int year = 0, month = 0, day = 0, hour = 0, minute = 0, second = 0, consumed = 0;
    if (sscanf(value.c_str(), "%4d-%2d-%2d %2d:%2d:%2d%n", &year, &month, &day, &hour, &minute, &second, &consumed) != 6
        || size_t(consumed) != value.size() || year < 1900 || month < 1 || month > 12 || day < 1 || day > 31
        || hour < 0 || hour > 23 || minute < 0 || minute > 59 || second < 0 || second > 59) return time_t(-1);
    parsed.tm_year = year - 1900; parsed.tm_mon = month - 1; parsed.tm_mday = day;
    parsed.tm_hour = hour; parsed.tm_min = minute; parsed.tm_sec = second; parsed.tm_isdst = -1;
    const auto result = mktime(&parsed);
    if (parsed.tm_year != year - 1900 || parsed.tm_mon != month - 1 || parsed.tm_mday != day
        || parsed.tm_hour != hour || parsed.tm_min != minute || parsed.tm_sec != second) return time_t(-1);
    return result;
}

string cast_time_to_datetime(time_t tiempo) {
	struct tm local_tm;
	struct tm* timeinfo;
	char buffer[80];
	timeinfo = localtime_r(&tiempo, &local_tm);
	strftime(buffer, 80, "%Y-%m-%d %H:%M:%S", timeinfo);
	string str(buffer);
	return str;
}

/*
string cast_double_to_string(double numerodouble) {
	char buffer1[40];
	memset(buffer1, 0, 40);
	sprintf(buffer1, "%f", numerodouble);
	string numerostr(buffer1);
	return numerostr;
}*/

string cast_double_to_string(double numerodouble, int decimales) {
    if (decimales < 0 || decimales > 100) throw std::invalid_argument("precision outside [0,100]");
    std::ostringstream out;
    out.imbue(std::locale::classic());
    out << std::fixed << std::setprecision(decimales) << numerodouble;
    return out.str();
}


string cast_float_to_string(float numerofloat) {
    return cast_double_to_string(numerofloat, 6);
}


string cast_int_to_string(int numeroint) {
	char buffer1[20];
	memset(buffer1, 0, 20);
	sprintf(buffer1, "%i", numeroint);
	string numerostr(buffer1);
	return numerostr;
}


string cast_bool_to_string(bool valor) {

	if (valor) {
		return "1";
	}
	else {
		return "0";
	}

}


int cast_string_to_int(string cadena1) { // convierte un valor cadena string a entero

	int valor = 0;

	// programa
	valor = atoi(cadena1.c_str());

	return valor;


}

// 1d4h8m25s
// x,xxx.xx
float cast_string_to_float(string cadena1) { // convierte un valor cadena string a float

	float valor = 0.0;
	float dias = 0.0;
	float horas = 0.0;
	float minutos = 0.0;
	float segundos = 0.0;
	float miles = 0.0;

	if (cadena1.compare("Alive") == 0) {
		return 1.0;
	}
	else if (cadena1.compare("Dead") == 0) {
		return 0.0;
	}

	string cadena2 = "";
	for (int i = 0; i < cadena1.length(); i++) {
		if (cadena1[i] != ',') {
			cadena2 = cadena2 + cadena1[i];
		}
		if (cadena1[i] == 'd') {
			dias = atof(cadena2.c_str()) * 1.0;
			cadena2 = "";
		}
		else if (cadena1[i] == 'h') {
			horas = atof(cadena2.c_str());
			cadena2 = "";
		}
		else if (cadena1[i] == 'm') {
			minutos = atof(cadena2.c_str());
			cadena2 = "";
		}
		else if (cadena1[i] == 's') {
			segundos = atof(cadena2.c_str());
			cadena2 = "";
		}
		else if (cadena1[i] == 'k') {
			miles = atof(cadena2.c_str());
			cadena2 = "";
		}

	}

	// programa

	if (cadena2.length() == 0) {
		valor = dias + (horas / 24.0) + (minutos / 1440.0) + (segundos / 86400.0) + miles * 1000.0;
	}
	else {
		valor = atof(cadena2.c_str());
	}

	return valor;
}


double cast_string_to_double(string cadena1) { // convierte un valor cadena string a float

	double valor = 0.0;
	// programa
	valor = atof(cadena1.c_str());

	return valor;
}



StringVector cast_string(const std::string& str, char separator)
{
	StringVector  result;
	size_t pos1 = 0;
	size_t pos2 = 0;
	while (pos2 != str.npos)
	{
		pos2 = str.find(separator, pos1);
		if (pos2 != str.npos)
		{
			if (pos2 > pos1)
				result.push_back(str.substr(pos1, pos2 - pos1));
			pos1 = pos2 + 1;
		}
	}
	result.push_back(str.substr(pos1, str.size() - pos1));
	return result;
}

string cast_arrchar_to_string(char* arr) {
	string s1;
	s1 = arr;
	return s1;
}

/* Get a float from 4 bytes (Modbus) in inversed format (DCBA) */
float convert_float_fp(float regVal)
{
	float PF_Val;

	if (regVal > 1)
	{
		PF_Val = 2 - regVal;
		//PF is leading
	}
	else if (regVal < -1)
	{
		PF_Val = -2 - regVal;
		//PF is leading
	}
	else if (regVal == 1.0)
	{
		PF_Val = regVal;
		//PF is at unity
	}
	else
	{
		PF_Val = regVal;
		//PF is lagging
	}
	return PF_Val;
}

std::string ReplaceAll(std::string str, const std::string& from, const std::string& to) {
	size_t start_pos = 0;
	while ((start_pos = str.find(from, start_pos)) != std::string::npos) {
		str.replace(start_pos, from.length(), to);
		start_pos += to.length(); // Handles case where 'to' is a substring of 'from'
	}
	return str;
}


std::vector<string> split1(const string& cadena, const char& delim) {
	std::vector<string> vector2;
	std::stringstream ss1;
	ss1 << cadena;
	std::string token;
	while (std::getline(ss1, token, delim)) {
		vector2.push_back(token);
	}
	return vector2;
}



int obtenervalorbit(int valor, int posicion = 0) {
	int res = 0;
	res = (valor >> posicion) & 0x1;
	return res;
}


long huella(string cadena) {
	int longitud = cadena.length();
	long acum = 0;
	for (int i = 0; i < longitud; i++) {
		acum += cadena[i] * (1 + i);


	}
	return acum;
}


string exec(const char* cmd) {
	char buffer[128];
	std::string result = "";
	// para linux es popen
	// para windows es _popen y solo para aplicacion de consola

#ifdef _WIN32
	FILE* pipe = _popen(cmd, "r");
#else
	FILE* pipe = popen(cmd, "r");
#endif
	
	if (!pipe) throw std::runtime_error("_popen() failed!");
	try {
		while (fgets(buffer, sizeof buffer, pipe) != NULL) {
			result += buffer;
		}
	}
	catch (...) {

#ifdef _WIN32
		_pclose(pipe);
#else
		pclose(pipe);
#endif
		throw;
	}
#ifdef _WIN32
	_pclose(pipe);
#else
	pclose(pipe);
#endif
	return result;
}


bool chequear_internet(string ipaddress1) {

	string res = "";
	string comando;
	size_t ubicacion;
	
#ifdef _WIN32
	comando = "ping -n 1 " + ipaddress1;
#else
	comando = "ping -c 1 " + ipaddress1;
#endif
	// en linux el comando es ping -c 1 1.0.0.1
	// en windows es ping -n 1 1.0.0.1

	res = exec(comando.c_str());

	ubicacion = res.find("time=", 0);

	if (ubicacion != std::string::npos) {
		return true;
	}
	else {
		return false;
	}
}


string cast_minus_mayus(string str) {
	string mayus;
	char C;
	for (int i = 0; i < str.length(); i++) {
		C = toupper(str[i]);
		mayus.append(1, C);
	}
	return mayus;
};


string cast_mayus_minus(string str) {
	string minus;
	char C;
	for (int i = 0; i < str.length(); i++) {
		C = tolower(str[i]);
		minus.append(1, C);
	}
	return minus;
};


bool cast_int_bool(int valor) {
	if (valor == 0) {
		return false;
	}
	else {
		return true;
	}
}

bool habilitarescribir(bool fueescrito, int solocambios) {
	return !(fueescrito && solocambios);
}


double funmenor(double v1, double v2) {
	return (v1 > v2) ? v2 : v1;
}


int converthex2dec(char num[]) {
	int len = strlen(num);
	int base = 1;
	int temp = 0;
	for (int i = len - 1; i >= 0; i--) {
		if (num[i] >= '0' && num[i] <= '9') {
			temp += (num[i] - 48) * base;
			base = base * 16;
		}
		else if (num[i] >= 'A' && num[i] <= 'F') {
			temp += (num[i] - 55) * base;
			base = base * 16;
		}
	}
	return temp;
}


string tratamientocadena(const string& entradacadena)
{
	string cadena="";
	string caracter = "";
	string caracterdec = "";
	char c;
	int numero;
	char numerohex[] = { 0,0,0 };
	string caracter2 = "";
	cadena.append(entradacadena);


	size_t pos = cadena.find("#");
	while (pos != string::npos) {

		caracter = cadena.substr(pos + 1, 2);
		caracter2 = "";
		sprintf(numerohex, "%s", caracter.c_str());
		numero = converthex2dec(numerohex);
		c = char(numero);
		caracter2.push_back(c);
		cadena.replace(pos, 3, caracter2);
		pos = cadena.find("#", pos);
	};

	return cadena;
}


string limpiarcadena(const string& cadenaoriginal) {
	string cadenanueva;
	int caracter;
	cadenanueva.empty();
	for (int i = 0; i < cadenaoriginal.length(); i++) {
		caracter = (int)cadenaoriginal[i];
		if (caracter > 32) {
			cadenanueva.append(1, cadenaoriginal[i]);
		}
		
	}
	return cadenanueva;
}


string fecha_para_emails() {
  string s1;
  const char * wday[7] = {"Sun","Mon","Tue","Wed","Thu","Fri","Sat"};
  const char * myear[12] = {"Jan","Feb","Mar","Apr","May","Jun","Jul","Aug","Sep","Oct","Nov","Dec"};  
  char buffer[32];  
  time_t rawtime;
  struct tm local_tm;
  struct tm * ptm;
  time ( &rawtime );
  ptm = gmtime_r(&rawtime, &local_tm);
  sprintf(buffer,"%s, %i %s %i %02d:%02d:%02d", wday[ptm->tm_wday],ptm->tm_mday,myear[ptm->tm_mon],ptm->tm_year+1900,
      ptm->tm_hour,ptm->tm_min,ptm->tm_sec);
		s1 = buffer;
	return s1;
}

string fecha_para_emails_local() {
  string s1;
  const char * wday[7] = {"Dom","Lun","Mar","Mie","Jue","Vie","Sab"};
  const char * myear[12] = {"Ene","Feb","Mar","Abr","May","Jun","Jul","Ago","Sep","Oct","Nov","Dic"};  
  char buffer[32];  
  time_t rawtime;
  struct tm local_tm;
  struct tm * ptm;
  time ( &rawtime );
  ptm = localtime_r(&rawtime, &local_tm); // gmtime_r(&rawtime, &local_tm)
  sprintf(buffer,"%s, %i %s %i %02d:%02d:%02d", wday[ptm->tm_wday],ptm->tm_mday,myear[ptm->tm_mon],ptm->tm_year+1900,
      ptm->tm_hour,ptm->tm_min,ptm->tm_sec);
		s1 = buffer;
	return s1;
}

string fecha_para_emails_local_es() {
  string s1;
  const char * wday[7] = {"Dom","Lun","Mar","Mie","Jue","Vie","Sab"};
  const char * myear[12] = {"Ene","Feb","Mar","Abr","May","Jun","Jul","Ago","Sep","Oct","Nov","Dic"};  
  char buffer[32];  
  time_t rawtime;
  struct tm local_tm;
  struct tm * ptm;
  time ( &rawtime );
  ptm = localtime_r(&rawtime, &local_tm); // gmtime_r(&rawtime, &local_tm)
  sprintf(buffer,"%s, %i %s %i %02d:%02d:%02d", wday[ptm->tm_wday],ptm->tm_mday,myear[ptm->tm_mon],ptm->tm_year+1900,
      ptm->tm_hour,ptm->tm_min,ptm->tm_sec);
		s1 = buffer;
	return s1;
}

string fecha_para_emails_utc_es() {
  string s1;
  const char * wday[7] = {"Dom","Lun","Mar","Mie","Jue","Vie","Sab"};
  const char * myear[12] = {"Ene","Feb","Mar","Abr","May","Jun","Jul","Ago","Sep","Oct","Nov","Dic"};  
  char buffer[32];  
  time_t rawtime;
  struct tm local_tm;
  struct tm * ptm;
  time ( &rawtime );
  ptm = gmtime_r(&rawtime, &local_tm); // gmtime_r(&rawtime, &local_tm)
  sprintf(buffer,"%s, %i %s %i %02d:%02d:%02d", wday[ptm->tm_wday],ptm->tm_mday,myear[ptm->tm_mon],ptm->tm_year+1900,
      ptm->tm_hour,ptm->tm_min,ptm->tm_sec);
		s1 = buffer;
	return s1;
}


int imprimirmac_from_ifrname(char* if_name){
	
  struct ifreq ifr = {};
  int s;
  if ((s = socket(AF_INET, SOCK_STREAM,0)) < 0) {
    perror("socket");
    return -1;
  }
  
  if (!if_name || strlen(if_name) >= IFNAMSIZ) { close(s); return -1; }
  memcpy(ifr.ifr_name, if_name, strlen(if_name) + 1);
  if (ioctl(s, SIOCGIFHWADDR, &ifr) < 0) {
    perror("ioctl");
    close(s);
    return -1;
  }
  
  unsigned char *hwaddr = (unsigned char *)ifr.ifr_hwaddr.sa_data;
  printf("%02X:%02X:%02X:%02X:%02X:%02X\n", hwaddr[0], hwaddr[1], hwaddr[2], hwaddr[3], hwaddr[4], hwaddr[5]);
  close(s);
  return 0;
}


unsigned long long valormac_from_ifrname(char* if_name){
	
  struct ifreq ifr = {};
  int s;
  unsigned long long valormac = 0;
  unsigned char mac[6];
  
  if ((s = socket(AF_INET, SOCK_STREAM,0)) < 0) {
    perror("socket");
    return -1;
  }
  
  if (!if_name || strlen(if_name) >= IFNAMSIZ) { close(s); return -1; }
  memcpy(ifr.ifr_name, if_name, strlen(if_name) + 1);
  if (ioctl(s, SIOCGIFHWADDR, &ifr) < 0) {
    perror("ioctl");
    close(s);
    return -1;
  }
  
  unsigned char *hwaddr = (unsigned char *)ifr.ifr_hwaddr.sa_data;
  mac[5] = hwaddr[0];
  mac[4] = hwaddr[1];
  mac[3] = hwaddr[2];
  mac[2] = hwaddr[3];
  mac[1] = hwaddr[4];
  mac[0] = hwaddr[5];
  memcpy(&valormac,&mac[0],sizeof(mac));
  close(s);
  return valormac;
	
}



int listainterfaces() {
	struct ifconf st_ifconf;
	struct ifreq *arrst_ifreq, st_ifreq;
	int s1;

	unsigned i;
	size_t len;
	int longitud = 0;
	unsigned char hwaddr[6];
	unsigned long long valormac;
	char buf[16384];
	

	st_ifconf.ifc_len=sizeof buf;
	st_ifconf.ifc_buf=buf;
	
	if ((s1 = socket(AF_INET, SOCK_DGRAM,0)) < 0) {
    perror("socket");
    return -1;
  }
  


	if(ioctl(s1, SIOCGIFCONF, &st_ifconf)!=0) {
		perror("ioctl(SIOCGIFCONF)");
		close(s1);
		return 0;
	}

	arrst_ifreq=st_ifconf.ifc_req;  //array de estructura 
		
	longitud = st_ifconf.ifc_len;
	
	printf("longitud = %i \n",longitud );
	
	for(i=0;i<longitud;) {

		len=sizeof(*arrst_ifreq);
		
		printf("%s ", arrst_ifreq->ifr_name);
		
		imprimirmac_from_ifrname(arrst_ifreq->ifr_name);
		valormac = valormac_from_ifrname(arrst_ifreq->ifr_name);
		printf ("serial decimal = %lld\n",valormac);
		//memcpy(&st_ifreq, arrst_ifreq,len);
		
		/* hwaddr[0] = (unsigned char) st_ifreq.ifr_hwaddr.sa_data[0];
		hwaddr[1] = (unsigned char) st_ifreq.ifr_hwaddr.sa_data[1];
		hwaddr[2] = (unsigned char) st_ifreq.ifr_hwaddr.sa_data[2];
		hwaddr[3] = (unsigned char) st_ifreq.ifr_hwaddr.sa_data[3];
		hwaddr[4] = (unsigned char) st_ifreq.ifr_hwaddr.sa_data[4];
		hwaddr[5] = (unsigned char) st_ifreq.ifr_hwaddr.sa_data[5];
	
		
		printf("%02X:%02X:%02X:%02X:%02X:%02X\n", hwaddr[0], hwaddr[1], hwaddr[2], hwaddr[3], hwaddr[4], hwaddr[5]); */ 
		
		arrst_ifreq=(struct ifreq*)((char*)arrst_ifreq+len);
		
		
		i+=len;	
			
	}
	
	close(s1);
	return 0;
}
