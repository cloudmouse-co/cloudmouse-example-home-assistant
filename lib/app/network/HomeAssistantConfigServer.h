#pragma once

#include <ESPmDNS.h>
#include <HTTPClient.h>

#include "../../utils/Logger.h"

#include "../services/HomeAssistantPrefs.h"
#include "../network/WebServerManager.h"

namespace CloudMouse::App::Network
{

    using namespace CloudMouse::App::Services;

    class HomeAssistantConfigServer
    {
    public:
        HomeAssistantConfigServer(HomeAssistantPrefs &preferences);
        ~HomeAssistantConfigServer();

        bool init();
        void update();

        bool hasValidSetup();
        bool hasValidConfig();

        String getConfigURL();

        void setConfigChangedCallback(std::function<void()> callback)
        {
            onConfigChanged = callback;
        }

    private:
        HomeAssistantPrefs &prefs;
        HTTPClient httpClient;

        WebServer *webServer; // Pointer to SDK's WebServer instance
        bool serverRunning;

        String mDNS = "cloudmouse-" + GET_DEVICE_ID();

        // Static instance pointer for callbacks
        static HomeAssistantConfigServer *instance;

        static void handleSetupPage();
        static void handleSetupSubmit();
        static void handleConfigPage();
        static void handleConfigSubmit();

        String generateSetupPage();
        String generateConfigPage();

        String generateEntityCheckboxes(const String &entitiesJson, const String &selectedJson);
        String generateErrorPage(const String &error);

        std::function<void()> onConfigChanged;

        void configChangedCallback()
        {
            if (onConfigChanged)
            {
                onConfigChanged();
            }
        }
    };
} // namespace CloudMouse::App::Network
