#pragma once

#include <HTTPClient.h>
#include <ArduinoJson.h>
#include "./HomeAssistantPrefs.h"
#include "../utils/Logger.h"
#include "../../core/Core.h"

namespace CloudMouse::App::Services
{
    class HomeAssistantDataService
    {
    public:
        HomeAssistantDataService(HomeAssistantPrefs &preferences) : prefs(preferences) {}

        ~HomeAssistantDataService() {}

        bool init()
        {
            APP_LOGGER("Initializing Data Service...");

            if (prefs.hasApiKey() && prefs.hasHost())
            {
                haBaseUrl = prefs.getApiKey();
                haToken = prefs.getHost();

                APP_LOGGER("✅ Data Service initialized gracefully!");
                return true;
            }

            APP_LOGGER("❌ Failed to inizialize Data Service!");
            return false;
        }

        bool callService(const String &domain, const String &service, const String &entityId = "")
        {
            if (!WiFi.isConnected())
            {
                APP_LOGGER("❌ WiFi not connected");
                return false;
            }

            Core::instance().getLEDManager()->setLoadingState(true);

            String url = haBaseUrl + "/api/services/" + domain + "/" + service;

            APP_LOGGER("🏠 Calling HA: %s\n", url.c_str());

            http.begin(url);
            http.addHeader("Authorization", "Bearer " + haToken);
            http.addHeader("Content-Type", "application/json");

            String payload = entityId.isEmpty() ? "{}" : "{\"entity_id\":\"" + entityId + "\"}";

            int httpCode = http.POST(payload);
            bool success = (httpCode == 200);

            Core::instance().getLEDManager()->setLoadingState(false);

            if (success)
            {
                APP_LOGGER("✅ HA call successful");
            }
            else
            {
                APP_LOGGER("❌ HA call failed: %d\n", httpCode);
            }

            http.end();
            return success;
        }

        // Quick actions
        bool openGate() { return callService("cover", "open_cover", "cover.cancello"); }
        bool closeShutters() { return callService("cover", "close_cover", "cover.serrande"); }
        bool lightsOff() { return callService("light", "turn_off"); }
        bool entranceLightOn() { return callService("light", "turn_on", "light.entrata"); }

        static String fetchEntityList(HomeAssistantPrefs &prefs)
        {
            HTTPClient http;
            String url = prefs.getHost() + "api/states";
            String bearer = "Bearer " + prefs.getApiKey();

            APP_LOGGER("🌐 GET %s", url.c_str());

            Core::instance().getLEDManager()->setLoadingState(true);

            // Make HTTP request
            http.begin(url);
            http.addHeader("Authorization", bearer);

            int httpCode = http.GET();

            if (httpCode != HTTP_CODE_OK)
            {
                String error = "HTTP error: " + String(httpCode);
                // handleApiError(httpCode, error);
                // APP_LOGGER("API ERROR %s", error.c_str());
                http.end();

                Core::instance().getLEDManager()->setLoadingState(false);

                return error;
            }
            else
            {

                // Read response
                String payload = http.getString();

                // APP_LOGGER("RECEIVED PAYLOAD: %s", payload.c_str());

                http.end();

                Core::instance().getLEDManager()->setLoadingState(false);

                return payload;
            }
        }

    private:
        HomeAssistantPrefs &prefs;
        HTTPClient http;

        String haBaseUrl;
        String haToken;
    };
}