#include <stdio.h>
#include <iostream>
#include <esp_event.h>
#include <esp_log.h>
#include <esp_system.h>
#include <nvs_flash.h>
#include <sys/param.h>
#include <memory>
#include <esp_heap_caps.h>
#include "webserver.hpp"

extern "C" void app_main(void)
{
    ESP_ERROR_CHECK(nvs_flash_init());
    WebServer webServer = WebServer();
    webServer.init();
    webServer.startHttpServer();
}
