#include <esp_http_server.h>
#include <esp_httpd_priv.h>
#include <cstring>
#include "helpers.hpp"

httpd_req_t* copyHttpdRequest(const httpd_req_t* other) {
    
    httpd_req_t* reqCopy = (httpd_req_t*)malloc(sizeof(httpd_req_t));

    if (!reqCopy)
        return nullptr;

    std::memcpy((void*)reqCopy, (void*)other, sizeof(httpd_req_t));
    httpd_req_aux_t* auxCopy = new httpd_req_aux_t();

    if (!auxCopy) {
        free(reqCopy);
        return nullptr;
    }

    std::memcpy((void*)auxCopy, (void*)other->aux, sizeof(httpd_req_aux_t));
    reqCopy->aux = auxCopy;

    return reqCopy;
}
