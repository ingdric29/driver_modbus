/*
	Modificacion hecha por: Ing. Javier Flores
	Fecha: 12/11/2024
*/

#include "include/cls_sms.h"
#include "include/globales.h"

cls_sms::cls_sms() {}

// 0 si todo ok, y !0 si hubo error
int cls_sms::enviar() {
    CURL *curl;
    CURLcode res =
        CURLE_FAILED_INIT; // Legacy sender remains disabled; never report uninitialized success.
    curl = curl_easy_init();
    if (curl) {
        curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, "POST");
        curl_easy_setopt(curl, CURLOPT_URL, clsurl_modem.c_str());
        curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
        curl_easy_setopt(curl, CURLOPT_DEFAULT_PROTOCOL, "https");
        struct curl_slist *headers = NULL;
        curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
        curl_mime *mime;
        curl_mimepart *part;
        mime = curl_mime_init(curl);
        part = curl_mime_addpart(mime);
        curl_mime_name(part, "phone");
        curl_mime_data(part, clsnumero.c_str(), CURL_ZERO_TERMINATED);
        part = curl_mime_addpart(mime);
        curl_mime_name(part, "message");
        curl_mime_data(part, clsasunto.c_str(), CURL_ZERO_TERMINATED);
        curl_easy_setopt(curl, CURLOPT_MIMEPOST, mime);
        //res = curl_easy_perform(curl);
        curl_mime_free(mime);

        if (res != CURLE_OK) { // CURLE_OK ES IGUA A 0 SI TODO OK
            fprintf(stderr, "[ AVISO ] El emisor SMS permanece deshabilitado: %s\n",
                    curl_easy_strerror(res));
            imprimir_mensaje("el url de modem es:" + clsurl_modem, rojo);
        }
    }
    curl_easy_cleanup(curl);
    return (int)res;
}
