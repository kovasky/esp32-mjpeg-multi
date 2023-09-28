// Standard ESP-IDF Headers
#include <esp_check.h>
#include <esp_httpd_priv.h>
#include <esp_log.h>
#include <esp_netif.h>
#include <esp_wifi.h>
#include <nvs_flash.h>

// Custom Headers
#include "helpers.hpp"
#include "webserver.hpp"

#define FACTORY_INDEX (-1)
#define PART_BOUNDARY "123456789000000000000987654321"
#define MAX_SESSIONS 5

SemaphoreHandle_t WebServer::sessionTasksMutex = xSemaphoreCreateMutex();
TaskHandle_t WebServer::cameraCaptureTaskHandle = nullptr;
TaskHandle_t WebServer::wifiReconnectTaskHandle = nullptr;
std::shared_ptr<WebServer> WebServer::self = nullptr;

WebServer::WebServer() : camera(std::make_unique<Camera>()),
                         serverHandle(std::make_shared<httpd_handle_t>()),
                         serverConfig(std::make_shared<httpd_config_t>()),
                         wifiInitConfig(std::make_shared<wifi_init_config_t>()),
                         wifiConfig(std::make_shared<wifi_config_t>()),
                         sessionTasks(std::make_shared<std::vector<TaskHandle_t*>>())
{
    self = std::make_shared<WebServer>(*this);
}

WebServer::WebServer(const WebServer &other)
{
    if (other.camera)
        camera = other.camera;
    if (other.serverHandle)
        serverHandle = other.serverHandle;
    if (other.serverConfig)
        serverConfig = other.serverConfig;
    if (other.wifiInitConfig)
        wifiInitConfig = other.wifiInitConfig;
    if (other.wifiConfig)
        wifiConfig = other.wifiConfig;
    if (other.sessionTasks)
        sessionTasks = other.sessionTasks;
}

void WebServer::init()
{
    ESP_ERROR_CHECK(nvs_flash_init());

    *this->serverConfig = HTTPD_DEFAULT_CONFIG();
    *this->wifiInitConfig = WIFI_INIT_CONFIG_DEFAULT();
    this->wifiConfig->sta = {
        .ssid = CONFIG_SSID,
        .password = CONFIG_PASSWORD,
    };

    this->camera->init();

    ESP_ERROR_CHECK(esp_netif_init());

    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_create_default_wifi_sta();

    ESP_ERROR_CHECK(esp_wifi_init(this->wifiInitConfig.get()));

    esp_event_handler_instance_t ctxAny;
    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT,
                                                        ESP_EVENT_ANY_ID,
                                                        WebServer::eventHandler,
                                                        (void *)this,
                                                        &ctxAny));
    esp_event_handler_instance_t ctxIP;
    ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT,
                                                        IP_EVENT_STA_GOT_IP,
                                                        WebServer::eventHandler,
                                                        (void *)this,
                                                        &ctxIP));

    ESP_ERROR_CHECK(esp_wifi_set_storage(WIFI_STORAGE_RAM));
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, this->wifiConfig.get()));
    ESP_ERROR_CHECK(esp_wifi_start());
}

void WebServer::startHttpServer()
{
    if (httpd_start(this->serverHandle.get(), this->serverConfig.get()) != ESP_OK)
        return;

    httpd_uri_t urimJPEG = {
        .uri = "/stream", 
        .method = HTTP_GET,
        .handler = &WebServer::mJPEGHandler,
        .user_ctx = nullptr,
    };

    httpd_register_uri_handler(*(this->serverHandle), &urimJPEG );

    xTaskCreatePinnedToCore(WebServer::cameraCaptureTask,"Capture Frame Task",4096,nullptr,2,&this->cameraCaptureTaskHandle,1);
}

void WebServer::eventHandler(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data)
{
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START)
    {
        if (esp_wifi_connect() != ESP_OK)
            return;
        
    }
    else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED)
    {
        xTaskCreatePinnedToCore(WebServer::wifiReconnectTask,"Wifi Reconnect Task", 4096, nullptr, 1, &wifiReconnectTaskHandle,1);
    }
    else if(event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_CONNECTED)
    {
        if(wifiReconnectTaskHandle != nullptr)
        {
            vTaskDelete(wifiReconnectTaskHandle);
            wifiReconnectTaskHandle = nullptr;
        }
    }
}

esp_err_t WebServer::mJPEGHandler(httpd_req_t *req)
{
    xSemaphoreTake(self->sessionTasksMutex,portMAX_DELAY);
    size_t size = self->sessionTasks->size();
    xSemaphoreGive(self->sessionTasksMutex);

    if(size < MAX_SESSIONS)
    {
        httpd_resp_set_type(req, "multipart/x-mixed-replace;boundary=" PART_BOUNDARY);
        httpd_req_t *copyReq = copyHttpdRequest(req);
        TaskHandle_t* taskHandle = new TaskHandle_t; 
        xTaskCreatePinnedToCore(WebServer::mJPEGStreamTask,"mJPEG Stream Task", 4096, (void *)copyReq, 1, taskHandle,1);
        xSemaphoreTake(self->sessionTasksMutex,portMAX_DELAY);
        self->sessionTasks->push_back(taskHandle);
        xSemaphoreGive(self->sessionTasksMutex);

        if(eTaskGetState(cameraCaptureTaskHandle) == eSuspended)
            vTaskResume(cameraCaptureTaskHandle);
    }
    else{
        static const char *response = "Can't take more clients, try again later.";
        httpd_resp_set_type(req, "text/plain");
        httpd_resp_send(req, response, strlen(response));
    }

    return ESP_OK;
}

void WebServer::mJPEGStreamTask(void* pvParameters)
{
    static const char *frame_boundary = "--" PART_BOUNDARY "\r\nContent-Type: image/jpeg\r\n\r\n";
    static camera_fb_t* fb = nullptr;
    static BaseType_t higherPriorityTaskWoken = pdTRUE;
    httpd_req_t *req = (httpd_req_t *)pvParameters;
    esp_err_t res = ESP_OK;

    while(true)
    {
        fb = *(camera_fb_t**)ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
        if (fb != nullptr)
        {
            if(req != nullptr && fb->len > 0)
            {
                res |= httpd_resp_send_chunk(req, frame_boundary, strlen(frame_boundary));
                res |= httpd_resp_send_chunk(req, (const char *)fb->buf, fb->len);
                res |= httpd_resp_send_chunk(req, "\r\n", 2);

                if(res != ESP_OK)
                {
                    break;
                }
            }
        }
        xTaskNotifyGive(self->cameraCaptureTaskHandle);
    }
    
    delete (httpd_req_aux_t*)req->aux;
    free(req);
    xTaskNotifyFromISR(self->cameraCaptureTaskHandle,UINT32_MAX,eSetValueWithOverwrite,&higherPriorityTaskWoken);
    vTaskSuspend(nullptr);
}

void WebServer::cameraCaptureTask(void* pvParameters)
{
    static camera_fb_t* fb = nullptr;
    static size_t size = 0;
    static BaseType_t higherPriorityTaskWoken = pdTRUE; // Initialize the flag
    static uint32_t taskStatus;

    while(true)
    {
        xSemaphoreTake(self->sessionTasksMutex,portMAX_DELAY);
        size = self->sessionTasks->size();
        xSemaphoreGive(self->sessionTasksMutex);

        if(size == 0)
            vTaskSuspend(nullptr);

        fb = self->camera->takePicture();

        if (fb == nullptr)
            continue;

        xSemaphoreTake(self->sessionTasksMutex,portMAX_DELAY);
        std::vector<TaskHandle_t*>::iterator it = self->sessionTasks->begin();
        while(it != self->sessionTasks->end())
        {
            xTaskNotifyFromISR(**it, (uint32_t)&fb,eSetValueWithOverwrite,&higherPriorityTaskWoken);
            vPortYield();
            
            taskStatus = ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
            if(taskStatus == UINT32_MAX)
            {
                vTaskDelete(**it);
                TaskHandle_t* handle = *it;
                it = self->sessionTasks->erase(it);
                delete handle;
                continue;
            }

            ++it;
        }
        xSemaphoreGive(self->sessionTasksMutex);
        
        if (fb != nullptr)
            self->camera->freeBuffer(fb);
        
        fb = nullptr;
        vTaskDelay(15 / portTICK_PERIOD_MS);
    }
}

void WebServer::wifiReconnectTask(void* pvParameters)
{
    while(esp_wifi_connect() != ESP_OK)
    {
        vTaskDelay(1000 / portTICK_PERIOD_MS);
    }

    vTaskSuspend(nullptr);
}
