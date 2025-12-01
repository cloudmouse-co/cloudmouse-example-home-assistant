#pragma once

#include <ArduinoJson.h>
#include "../../utils/Logger.h"

namespace CloudMouse::App
{
    class HomeAssistantEntity
    {
    private:
        JsonDocument doc; // ArduinoJson handles PSRAM automatically

    public:
        bool parse(const String &payload)
        {
            DeserializationError error = deserializeJson(doc, payload);
            if (error)
            {
                APP_LOGGER("JSON parse failed: %s", error.c_str());
                return false;
            }
            return true;
        }

        // Easy accessors for your climate entity
        const char *getEntityId() { return doc["entity_id"]; }
        const char *getState() { return doc["state"]; }
        const char *getFriendlyName() { return doc["attributes"]["friendly_name"]; }

        JsonVariant getAttributes()
        {
            return doc["attributes"];
        }

        // Bonus: direct attribute access helper
        JsonVariant getAttribute(const char *key)
        {
            return doc["attributes"][key];
        }
    };
}
