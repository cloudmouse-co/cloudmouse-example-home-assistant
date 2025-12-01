#pragma once

#include <HTTPClient.h>
#include <ArduinoJson.h>
#include "./HomeAssistantPrefs.h"

namespace CloudMouse::App::Services
{
    class HomeAssistantDataService
    {
    public:
        HomeAssistantDataService(HomeAssistantPrefs &preferences) : prefs(preferences) {}
        ~HomeAssistantDataService() {}

        bool init();

        bool callService(const String &domain, const String &service, const String &entityId = "");

        bool fetchEntityStatus(const String entity_id);

        // Quick actions
        bool openGate();
        bool closeShutters();
        bool lightsOff();
        bool entranceLightOn();
        bool setSwitchOn(const String &entityId);
        bool setSwitchOff(const String &entityId);

        static String fetchEntityList(HomeAssistantPrefs &prefs);

    private:
        HomeAssistantPrefs &prefs;
        HTTPClient http;

        String haBaseUrl;
        String haToken;
    };
}