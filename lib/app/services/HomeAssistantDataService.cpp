#pragma once

#include "../utils/Logger.h"
#include "../../core/Core.h"
#include "../model/HomeAssistantAppStore.h"
#include "../HomeAssistantApp.h"

namespace CloudMouse::App::Services
{

    using namespace CloudMouse::App;

        bool HomeAssistantDataService::init()
        {
            APP_LOGGER("Initializing Data Service...");

            if (prefs.hasApiKey() && prefs.hasHost() && prefs.getPort())
            {
                haBaseUrl = "http://" + prefs.getHost() + ":" + prefs.getPort();
                haToken = prefs.getApiKey();

                APP_LOGGER("✅ Data Service initialized gracefully!");
                return true;
            }

            APP_LOGGER("❌ Failed to inizialize Data Service!");
            return false;
        }

        bool HomeAssistantDataService::callService(const String &domain, const String &service, const String &entityId)
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
                // String payload = http.getString();
                APP_LOGGER("✅ HA call successful");
                // APP_LOGGER("Payload received: %s", payload.c_str());

                // AppStore::instance().setEntity(entityId, payload);
                // CloudMouse::EventBus::instance().sendToUI(toSDKEvent(AppEventData::entityUpdated(entityId)));
            }
            else
            {
                APP_LOGGER("❌ HA call failed: %d\n", httpCode);
            }

            http.end();
            return success;
        }

        bool HomeAssistantDataService::fetchEntityStatus(const String entity_id)
        {
            if (!WiFi.isConnected())
            {
                APP_LOGGER("❌ WiFi not connected");
                return false;
            }

            Core::instance().getLEDManager()->setLoadingState(true);

            String url = haBaseUrl + "/api/states/" + entity_id;

            APP_LOGGER("🏠 Calling HA: %s\n", url.c_str());

            http.begin(url);
            http.addHeader("Authorization", "Bearer " + haToken);
            http.addHeader("Content-Type", "application/json");

            int httpCode = http.GET();
            bool success = (httpCode == 200);

            if (success) {
                String payload = http.getString();
                APP_LOGGER("✅ HA call successful");
                APP_LOGGER("Payload received: %s", payload.c_str());

                AppStore::instance().setEntity(entity_id, payload);
            } else {

                APP_LOGGER("❌ HA call failed: %d\n", httpCode);
            }

            http.end();
            
            Core::instance().getLEDManager()->setLoadingState(false);

            return success;
        }

        // Quick actions
        bool HomeAssistantDataService::openGate() { return callService("cover", "open_cover", "cover.cancello"); }
        bool HomeAssistantDataService::closeShutters() { return callService("cover", "close_cover", "cover.serrande"); }
        bool HomeAssistantDataService::lightsOff() { return callService("light", "turn_off"); }
        bool HomeAssistantDataService::entranceLightOn() { return callService("light", "turn_on", "light.entrata"); }
        bool HomeAssistantDataService::setSwitchOn(const String &entityId) { return callService("switch", "turn_on", entityId); }
        bool HomeAssistantDataService::setSwitchOff(const String &entityId) { return callService("switch", "turn_off", entityId); }
        bool HomeAssistantDataService::setLightOn(const String &entityId) { return callService("light", "turn_on", entityId); }
        bool HomeAssistantDataService::setLightOff(const String &entityId) { return callService("light", "turn_off", entityId); }

        String HomeAssistantDataService::fetchEntityList(HomeAssistantPrefs &prefs)
        {
            HTTPClient http;
            String url = "http://" + prefs.getHost() + ":" + prefs.getPort() + "/api/states";
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


}