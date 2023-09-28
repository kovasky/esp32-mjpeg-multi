#include <esp_event.h>
#include <esp_log.h>
#include <esp_check.h>
#include "camera.hpp"

static const char *TAG = "";

esp_err_t Camera::init()
{
    camera_config_t cameraConfig = {
        .pin_pwdn = CONFIG_PWDN,
        .pin_reset = CONFIG_RESET,
        .pin_xclk = CONFIG_XCLK,
        .pin_sscb_sda = CONFIG_SDA,
        .pin_sscb_scl = CONFIG_SCL,
        .pin_d7 = CONFIG_D7,
        .pin_d6 = CONFIG_D6,
        .pin_d5 = CONFIG_D5,
        .pin_d4 = CONFIG_D4,
        .pin_d3 = CONFIG_D3,
        .pin_d2 = CONFIG_D2,
        .pin_d1 = CONFIG_D1,
        .pin_d0 = CONFIG_D0,
        .pin_vsync = CONFIG_VSYNC,
        .pin_href = CONFIG_HREF,
        .pin_pclk = CONFIG_PCLK,
        .xclk_freq_hz = CONFIG_XCLK_FREQ,
        .ledc_timer = LEDC_TIMER_0,
        .ledc_channel = LEDC_CHANNEL_0,
        .pixel_format = PIXFORMAT_JPEG,
        .frame_size = FRAMESIZE_SXGA,
        .jpeg_quality = 30,
        .fb_count = 2,
        .fb_location = CAMERA_FB_IN_PSRAM,
        .grab_mode = CAMERA_GRAB_LATEST
    };

    ESP_RETURN_ON_ERROR(esp_camera_init(&cameraConfig), TAG, "Failed to initialize esp-camera");
    ESP_LOGI(TAG,"Camera Initialization Complete");
    return ESP_OK;
}

camera_fb_t* Camera::takePicture()
{
    return esp_camera_fb_get();
}

void Camera::freeBuffer(camera_fb_t* fb)
{
    esp_camera_fb_return(fb);
}
