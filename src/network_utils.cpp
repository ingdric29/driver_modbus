// Original implementation: Alejandro Diomede / Automatroni; maintenance: Javier Flores.
// Function bodies and platform alternatives preserved during modularization.
#include "include/conversiones.h"
#include <cstring>
#include <stdexcept>
#include <iomanip>
#include <locale>

using namespace std;

string exec(const char *cmd) {
    char buffer[128];
    std::string result = "";
    // para linux es popen
    // para windows es _popen y solo para aplicacion de consola

#ifdef _WIN32
    FILE *pipe = _popen(cmd, "r");
#else
    FILE *pipe = popen(cmd, "r");
#endif

    if (!pipe)
        throw std::runtime_error("_popen() failed!");
    try {
        while (fgets(buffer, sizeof buffer, pipe) != NULL) {
            result += buffer;
        }
    } catch (...) {

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
    } else {
        return false;
    }
}

int imprimirmac_from_ifrname(char *if_name) {

    struct ifreq ifr = {};
    int s;
    if ((s = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        perror("socket");
        return -1;
    }

    if (!if_name || strlen(if_name) >= IFNAMSIZ) {
        close(s);
        return -1;
    }
    memcpy(ifr.ifr_name, if_name, strlen(if_name) + 1);
    if (ioctl(s, SIOCGIFHWADDR, &ifr) < 0) {
        perror("ioctl");
        close(s);
        return -1;
    }

    unsigned char *hwaddr = (unsigned char *)ifr.ifr_hwaddr.sa_data;
    printf("%02X:%02X:%02X:%02X:%02X:%02X\n", hwaddr[0], hwaddr[1], hwaddr[2], hwaddr[3], hwaddr[4],
           hwaddr[5]);
    close(s);
    return 0;
}

unsigned long long valormac_from_ifrname(char *if_name) {

    struct ifreq ifr = {};
    int s;
    unsigned long long valormac = 0;
    unsigned char mac[6];

    if ((s = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        perror("socket");
        return -1;
    }

    if (!if_name || strlen(if_name) >= IFNAMSIZ) {
        close(s);
        return -1;
    }
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
    memcpy(&valormac, &mac[0], sizeof(mac));
    close(s);
    return valormac;
}

int listainterfaces() {
    struct ifconf st_ifconf;
    struct ifreq *arrst_ifreq;
    int s1;

    size_t i;
    size_t len;
    int longitud = 0;
    unsigned long long valormac;
    char buf[16384];

    st_ifconf.ifc_len = sizeof buf;
    st_ifconf.ifc_buf = buf;

    if ((s1 = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
        perror("socket");
        return -1;
    }

    if (ioctl(s1, SIOCGIFCONF, &st_ifconf) != 0) {
        perror("ioctl(SIOCGIFCONF)");
        close(s1);
        return 0;
    }

    arrst_ifreq = st_ifconf.ifc_req; //array de estructura

    longitud = st_ifconf.ifc_len;

    printf("longitud = %i \n", longitud);

    for (i = 0; longitud >= 0 && i + sizeof(*arrst_ifreq) <= static_cast<size_t>(longitud);) {

        len = sizeof(*arrst_ifreq);

        printf("%s ", arrst_ifreq->ifr_name);

        imprimirmac_from_ifrname(arrst_ifreq->ifr_name);
        valormac = valormac_from_ifrname(arrst_ifreq->ifr_name);
        printf("serial decimal = %lld\n", valormac);
        //memcpy(&st_ifreq, arrst_ifreq,len);

        arrst_ifreq = (struct ifreq *)((char *)arrst_ifreq + len);

        i += len;
    }

    close(s1);
    return 0;
}
