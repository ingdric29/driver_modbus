#include <stdio.h>
#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>
#include <curl/curl.h>
#include <boost/filesystem.hpp>
#include "include/estructuras.h"
#include "include/globales.h"
#include "include/conversiones.h"
#include "include/cls_logger.h"
#include <chrono>
#include <random>

#define SKIP_PEER_VERIFICATION 1

using namespace std;

Logger email_log;

std::string dir_plantilla_html = obtenerDirectorio("html") + "/" + "plantilla_emails.html";
std::string dir_imagen = obtenerDirectorio("html") + "/" + "logo_automonitor.png";

std::string generar_message_id(const std::string& from_email) {
    // Obtener marca de tiempo en nanosegundos para asegurar unicidad temporal
    auto ahora = std::chrono::high_resolution_clock::now();
    auto nanos = std::chrono::duration_cast<std::chrono::nanoseconds>(ahora.time_since_epoch()).count();

    // Generar un número aleatorio para evitar colisiones
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<long long> dis(100000, 999999);
    long long rand_num = dis(gen);

    // Extraer el dominio del correo 'from' (ejemplo: user@tu-dominio.com -> tu-dominio.com)
    std::string dominio = "automonitor.local"; // Dominio por defecto si falla la extracción
    size_t pos = from_email.find('@');
    if (pos != std::string::npos && pos + 1 < from_email.length()) {
        dominio = from_email.substr(pos + 1);
        // Limpiar espacios si el campo 'from' incluye nombres adicionales
        size_t fin_dominio = dominio.find_first_of(" >\t");
        if (fin_dominio != std::string::npos) {
            dominio = dominio.substr(0, fin_dominio);
        }
    }

    return "<" + std::to_string(nanos) + "." + std::to_string(rand_num) + "@" + dominio + ">";
}


string cargarPlantillaHTML(const string& ruta) {
    ifstream archivo(ruta);
    if (!archivo) {
        email_log.registrarError("ERROR: No se pudo abrir la plantilla HTML.");
        return "";
    }
    stringstream buffer;
    buffer << archivo.rdbuf();
    return buffer.str();
}

void replaceAll(std::string &str, const std::string &from, const std::string &to) {
    // 1. Evitar bucle infinito si 'from' está vacío
    if (from.empty()) {
        return;
    }

    size_t start_pos = 0;
    while ((start_pos = str.find(from, start_pos)) != std::string::npos) {
        str.replace(start_pos, from.length(), to);
        start_pos += to.length(); // Avanzar posición
    }
}

static size_t payload_source(void *ptr, size_t size, size_t nmemb, void *userp) {
    upload_status *upload_ctx = (upload_status *)userp;
    if (!upload_ctx || !upload_ctx->payload) return 0;

    vector<string>& payload = *upload_ctx->payload;

    if (upload_ctx->lines_read >= payload.size()) {
        return 0;
    }

    const string &data = payload[upload_ctx->lines_read++];
    size_t len = data.size();
    memcpy(ptr, data.c_str(), len);
    return len;
}


int enviar_email(storigen origen, stdestino destino) {
    string html_template = cargarPlantillaHTML(dir_plantilla_html);
    
    if (html_template.empty()) {
        email_log.registrarError("ERROR: HTML vacío");
        return -1;
    }
	
	else{
		replaceAll(html_template, "{{system_name}}", GLOBAL_nombre_concentrador);
		replaceAll(html_template, "{{alarm_date}}", fecha_para_emails_local());
		replaceAll(html_template, "{{alarm_description}}", destino.cuerpo2.c_str());
        replaceAll(html_template, "{{alarm_value}}", destino.cuerpo3.c_str());
		replaceAll(html_template, "{{value_unit}}", destino.cuerpo4.c_str());
	}
	
	std::string message_id_str = "Message-ID: " + generar_message_id(origen.from) + "\r\n";

    vector<string> payload_text = {
        "Date: " + fecha_para_emails_local() + "\r\n",
        "To: " + destino.to + "\r\n",
        "From: " + origen.from + " (Alarma " + GLOBAL_nombre_concentrador + ")\r\n",
        "Cc: " + destino.cc + " (Usuario)\r\n",
        message_id_str,
        "Reply-To: " + origen.from + "\r\n",
        "Subject: " + destino.asunto + "\r\n",
        "MIME-Version: 1.0\r\n",
        "Content-Type: multipart/related; boundary=\"MIME_boundary\"\r\n",
        "\r\n",
        "--MIME_boundary\r\n",
        "Content-Type: text/html; charset=UTF-8\r\n",
        "Content-Transfer-Encoding: 7bit\r\n",
        "\r\n",
        html_template + "\r\n"
    };

    payload_text.push_back("\r\n--MIME_boundary--");

    imprimir_mensaje("Preparando email para enviar...");

    CURL *curl;
    CURLcode res = CURLE_OK;
    struct curl_slist *recipients = NULL;
    upload_status upload_ctx = {0, &payload_text};

    curl = curl_easy_init();
    if (curl) {
        curl_easy_setopt(curl, CURLOPT_USERNAME, origen.username.c_str());
        curl_easy_setopt(curl, CURLOPT_PASSWORD, origen.password.c_str());
        curl_easy_setopt(curl, CURLOPT_URL, origen.smtp.c_str());

    #ifdef SKIP_PEER_VERIFICATION
        curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L);
    #endif

        curl_easy_setopt(curl, CURLOPT_MAIL_FROM, origen.from.c_str());
        recipients = curl_slist_append(recipients, destino.to.c_str());
        if (!destino.cc.empty()) {
            recipients = curl_slist_append(recipients, destino.cc.c_str());
        }
        curl_easy_setopt(curl, CURLOPT_MAIL_RCPT, recipients);

        curl_easy_setopt(curl, CURLOPT_READFUNCTION, payload_source);
        curl_easy_setopt(curl, CURLOPT_READDATA, &upload_ctx);
        curl_easy_setopt(curl, CURLOPT_UPLOAD, 1L);
        curl_easy_setopt(curl, CURLOPT_VERBOSE, 0L);
        curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, 3L);
        curl_easy_setopt(curl, CURLOPT_TIMEOUT, 10L);

        res = curl_easy_perform(curl);

        if (res != CURLE_OK) {
            fprintf(stderr, "curl_easy_perform() failed: %s\n", curl_easy_strerror(res));
            email_log.registrarError(std::string("curl_easy_perform() failed:") + curl_easy_strerror(res));
        }

        curl_slist_free_all(recipients);
        curl_easy_cleanup(curl);
    }

    return static_cast<int>(res);
}