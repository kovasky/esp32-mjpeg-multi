#pragma once

#include "esp_camera.h"

class WebServer;

class Camera
{
public:
    Camera(){};
private:
    esp_err_t init();
    camera_fb_t* takePicture();
    void freeBuffer(camera_fb_t*);
friend class WebServer;
};
