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

#include <algorithm>
#include <cstring>
#include <iomanip>
#include <limits>
#include <locale>

#define SKIP_PEER_VERIFICATION 1

using namespace std;

Logger email_log;

std::string dir_plantilla_html = obtenerDirectorio("html") + "/" + "plantilla_emails.html";
std::string dir_imagen = obtenerDirectorio("html") + "/" + "logo_automonitor.png";

std::string generar_message_id(const std::string &from_email) {
    // Obtener marca de tiempo en nanosegundos para asegurar unicidad temporal
    auto ahora = std::chrono::high_resolution_clock::now();
    auto nanos =
        std::chrono::duration_cast<std::chrono::nanoseconds>(ahora.time_since_epoch()).count();

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

string cargarPlantillaHTML(const string &ruta) {
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
    auto *upload = static_cast<upload_status *>(userp);
    if (!upload || !upload->payload)
        return 0;
    if (size && nmemb > std::numeric_limits<size_t>::max() / size)
        return CURL_READFUNC_ABORT;
    const size_t capacity = size * nmemb;
    size_t written = 0;
    while (written < capacity && upload->lines_read < upload->payload->size()) {
        const auto &line = (*upload->payload)[upload->lines_read];
        const size_t count = std::min(capacity - written, line.size() - upload->line_offset);
        std::memcpy(static_cast<char *>(ptr) + written, line.data() + upload->line_offset, count);
        written += count;
        upload->line_offset += count;
        if (upload->line_offset == line.size()) {
            ++upload->lines_read;
            upload->line_offset = 0;
        }
    }
    return written;
}

namespace {
std::string safe_detail(std::string detail, const storigen &origin) {
    replaceAll(detail, origin.password, "[oculto]");
    replaceAll(detail, origin.username, "[oculto]");
    for (char &c : detail)
        if (static_cast<unsigned char>(c) < 32 || c == 127)
            c = ' ';
    return detail.substr(0, 512);
}
std::string smtp_context(const stdestino &destination) {
    std::string context = destination.context;
    for (char &c : context)
        if (static_cast<unsigned char>(c) < 32 || c == 127)
            c = ' ';
    return context.substr(0, 256);
}
void log_email_result(CURL *curl, CURLcode result, const storigen &origen,
                      const stdestino &destino) {
    const std::string context = smtp_context(destino);
    long smtp_code = 0;
    double dns = 0, tcp = 0, tls = 0, total = 0;
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &smtp_code);
    curl_easy_getinfo(curl, CURLINFO_NAMELOOKUP_TIME, &dns);
    curl_easy_getinfo(curl, CURLINFO_CONNECT_TIME, &tcp);
    curl_easy_getinfo(curl, CURLINFO_APPCONNECT_TIME, &tls);
    curl_easy_getinfo(curl, CURLINFO_TOTAL_TIME, &total);
    const std::string stage = result == CURLE_COULDNT_RESOLVE_HOST ? "DNS"
                              : result == CURLE_LOGIN_DENIED       ? "AUTH"
                              : tcp == 0                           ? "CONEXION"
                              : tls == 0                           ? "TLS"
                                                                   : "SMTP";
    std::ostringstream log;
    log.imbue(std::locale::classic());
    log << (result == CURLE_OK ? "INFO: SMTP: " : "ERROR: SMTP: ") << context
        << " modo=EMISOR_ORIGINAL"
        << " limites=libcurl_original"
        << " codigo=" << result << " smtp=" << smtp_code << std::fixed << std::setprecision(1)
        << " dns_ms=" << dns * 1000 << " conexion_ms=" << tcp * 1000 << " tls_ms=" << tls * 1000
        << " total_ms=" << total * 1000;
    if (result == CURLE_OK)
        log << " aceptado por el servidor";
    else
        log << " etapa=" << stage << " detalle=" << safe_detail(curl_easy_strerror(result), origen);
    email_log.registrarError(log.str());
}
} // namespace

int enviar_email(storigen origen, stdestino destino) {
    string html_template = cargarPlantillaHTML(dir_plantilla_html);

    if (html_template.empty()) {
        email_log.registrarError("ERROR: HTML vacío");
        return -1;
    }

    else {
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
        html_template + "\r\n"};

    payload_text.push_back("\r\n--MIME_boundary--");

    imprimir_mensaje("Preparando email para enviar...");

    CURL *curl;
    CURLcode res = CURLE_FAILED_INIT;
    struct curl_slist *recipients = NULL;
    upload_status upload_ctx = {0, &payload_text, 0};

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

        res = curl_easy_perform(curl);

        log_email_result(curl, res, origen, destino);

        curl_slist_free_all(recipients);
        curl_easy_cleanup(curl);
    }

    if (!curl)
        email_log.registrarError("ERROR: SMTP: " + smtp_context(destino) +
                                 " no se pudo inicializar cURL");
    return static_cast<int>(res);
}
