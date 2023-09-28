// Custom Headers
#include "webserver.hpp"

extern "C" void app_main(void)
{
    WebServer webServer = WebServer();
    webServer.init();
    webServer.startHttpServer();
}
