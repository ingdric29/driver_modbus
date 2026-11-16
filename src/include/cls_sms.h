#ifndef CLS_SMS

#define CLS_SMS

#include <string>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <curl/curl.h>
#include <iostream>

using namespace std;

class cls_sms {
  public:
    //parametros

    int clsid;
    string clsnumero;
    string clsasunto;
    string clsurl_modem;
    string clsparametros_rest_api;
    string clsmetodohtml;

    cls_sms();
    int enviar();
};

#endif
