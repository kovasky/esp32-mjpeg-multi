file(GLOB MSRCS *.cpp)

# Initialize a list to store all the directories
set(MY_INCLUDE_DIRS
    "${IDF_PATH}/components/esp_timer/include"
    "${IDF_PATH}/components/esp_http_server/src"
    "${IDF_PATH}/components/esp_http_server/src/port/esp32"
)

idf_component_register(SRCS ${MSRCS}
                    INCLUDE_DIRS "."
                    INCLUDE_DIRS "${MY_INCLUDE_DIRS}"
                    REQUIRES esp32-camera esp_wifi nvs_flash esp_http_server esp_netif)
