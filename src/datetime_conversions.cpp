// Original implementation: Alejandro Diomede / Automatroni; maintenance: Javier Flores.
// Function bodies and platform alternatives preserved during modularization.
#include "include/conversiones.h"
#include <cstring>
#include <stdexcept>
#include <iomanip>
#include <locale>

using namespace std;

time_t cast_datetime_to_time(const string &value) {
    std::tm parsed{};
    int year = 0, month = 0, day = 0, hour = 0, minute = 0, second = 0, consumed = 0;
    if (sscanf(value.c_str(), "%4d-%2d-%2d %2d:%2d:%2d%n", &year, &month, &day, &hour, &minute,
               &second, &consumed) != 6 ||
        size_t(consumed) != value.size() || year < 1900 || month < 1 || month > 12 || day < 1 ||
        day > 31 || hour < 0 || hour > 23 || minute < 0 || minute > 59 || second < 0 || second > 59)
        return time_t(-1);
    parsed.tm_year = year - 1900;
    parsed.tm_mon = month - 1;
    parsed.tm_mday = day;
    parsed.tm_hour = hour;
    parsed.tm_min = minute;
    parsed.tm_sec = second;
    parsed.tm_isdst = -1;
    const auto result = mktime(&parsed);
    if (parsed.tm_year != year - 1900 || parsed.tm_mon != month - 1 || parsed.tm_mday != day ||
        parsed.tm_hour != hour || parsed.tm_min != minute || parsed.tm_sec != second)
        return time_t(-1);
    return result;
}

string cast_time_to_datetime(time_t tiempo) {
    struct tm local_tm;
    struct tm *timeinfo;
    char buffer[80];
    timeinfo = localtime_r(&tiempo, &local_tm);
    strftime(buffer, 80, "%Y-%m-%d %H:%M:%S", timeinfo);
    string str(buffer);
    return str;
}

string fecha_para_emails() {
    string s1;
    const char *wday[7] = {"Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"};
    const char *myear[12] = {"Jan", "Feb", "Mar", "Apr", "May", "Jun",
                             "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"};
    char buffer[32];
    time_t rawtime;
    struct tm local_tm;
    struct tm *ptm;
    time(&rawtime);
    ptm = gmtime_r(&rawtime, &local_tm);
    sprintf(buffer, "%s, %i %s %i %02d:%02d:%02d", wday[ptm->tm_wday], ptm->tm_mday,
            myear[ptm->tm_mon], ptm->tm_year + 1900, ptm->tm_hour, ptm->tm_min, ptm->tm_sec);
    s1 = buffer;
    return s1;
}

string fecha_para_emails_local() {
    string s1;
    const char *wday[7] = {"Dom", "Lun", "Mar", "Mie", "Jue", "Vie", "Sab"};
    const char *myear[12] = {"Ene", "Feb", "Mar", "Abr", "May", "Jun",
                             "Jul", "Ago", "Sep", "Oct", "Nov", "Dic"};
    char buffer[32];
    time_t rawtime;
    struct tm local_tm;
    struct tm *ptm;
    time(&rawtime);
    ptm = localtime_r(&rawtime, &local_tm); // gmtime_r(&rawtime, &local_tm)
    sprintf(buffer, "%s, %i %s %i %02d:%02d:%02d", wday[ptm->tm_wday], ptm->tm_mday,
            myear[ptm->tm_mon], ptm->tm_year + 1900, ptm->tm_hour, ptm->tm_min, ptm->tm_sec);
    s1 = buffer;
    return s1;
}

string fecha_para_emails_local_es() {
    string s1;
    const char *wday[7] = {"Dom", "Lun", "Mar", "Mie", "Jue", "Vie", "Sab"};
    const char *myear[12] = {"Ene", "Feb", "Mar", "Abr", "May", "Jun",
                             "Jul", "Ago", "Sep", "Oct", "Nov", "Dic"};
    char buffer[32];
    time_t rawtime;
    struct tm local_tm;
    struct tm *ptm;
    time(&rawtime);
    ptm = localtime_r(&rawtime, &local_tm); // gmtime_r(&rawtime, &local_tm)
    sprintf(buffer, "%s, %i %s %i %02d:%02d:%02d", wday[ptm->tm_wday], ptm->tm_mday,
            myear[ptm->tm_mon], ptm->tm_year + 1900, ptm->tm_hour, ptm->tm_min, ptm->tm_sec);
    s1 = buffer;
    return s1;
}

string fecha_para_emails_utc_es() {
    string s1;
    const char *wday[7] = {"Dom", "Lun", "Mar", "Mie", "Jue", "Vie", "Sab"};
    const char *myear[12] = {"Ene", "Feb", "Mar", "Abr", "May", "Jun",
                             "Jul", "Ago", "Sep", "Oct", "Nov", "Dic"};
    char buffer[32];
    time_t rawtime;
    struct tm local_tm;
    struct tm *ptm;
    time(&rawtime);
    ptm = gmtime_r(&rawtime, &local_tm); // gmtime_r(&rawtime, &local_tm)
    sprintf(buffer, "%s, %i %s %i %02d:%02d:%02d", wday[ptm->tm_wday], ptm->tm_mday,
            myear[ptm->tm_mon], ptm->tm_year + 1900, ptm->tm_hour, ptm->tm_min, ptm->tm_sec);
    s1 = buffer;
    return s1;
}
