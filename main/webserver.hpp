#pragma once

// STL Headers
#include <memory>
#include <vector>

// Standard ESP-IDF Headers
#include <esp_wifi.h>
#include <esp_netif.h>

// Custom Headers
#include "camera.hpp"
#include "helpers.hpp"

class WebServer
{
public:
    WebServer();
    WebServer(const WebServer& other);
    void init();
    void startHttpServer();

private:
    static SemaphoreHandle_t sessionTasksMutex;
    static TaskHandle_t cameraCaptureTaskHandle;
    static std::shared_ptr<WebServer> self;
    static void eventHandler(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data);
    static esp_err_t mJPEGHandler(httpd_req_t *req);
    static void mJPEGStreamTask(void* pvParameters);
    static void cameraCaptureTask(void* pvParameters);
    std::shared_ptr<Camera> camera;
    std::shared_ptr<httpd_handle_t> serverHandle;
    std::shared_ptr<httpd_config_t> serverConfig;
    std::shared_ptr<wifi_init_config_t> wifiInitConfig;
    std::shared_ptr<wifi_config_t> wifiConfig;
    std::shared_ptr<std::vector<TaskHandle_t*>> sessionTasks;
};
