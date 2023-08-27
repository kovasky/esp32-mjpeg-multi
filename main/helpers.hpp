#pragma once

#include <esp_http_server.h>

httpd_req_t* copyHttpdRequest(const httpd_req_t* other);
