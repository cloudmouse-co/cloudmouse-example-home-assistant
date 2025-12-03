#pragma once

#include <ArduinoJson.h>
#include "../../utils/Logger.h"

namespace CloudMouse::App
{
    class HomeAssistantEntity
    {
    private:
        JsonDocument* doc;  // Pointer instead of direct object

    public:
        HomeAssistantEntity() {
            // Allocate JsonDocument in PSRAM
            doc = (JsonDocument*)ps_malloc(sizeof(JsonDocument));
            new (doc) JsonDocument();  // Placement new
        }

        ~HomeAssistantEntity() {
            if (doc) {
                doc->~JsonDocument();  // Manual destructor
                free(doc);
            }
        }

        bool parse(const String &payload)
        {
            DeserializationError error = deserializeJson(*doc, payload);
            if (error)
            {
                APP_LOGGER("JSON parse failed: %s", error.c_str());
                return false;
            }
            return true;
        }

        const char *getEntityId() { return (*doc)["entity_id"]; }
        const char *getState() { return (*doc)["state"]; }
        const char *getFriendlyName() { return (*doc)["attributes"]["friendly_name"]; }

        JsonVariant getAttributes()
        {
            return (*doc)["attributes"];
        }

        JsonVariant getAttribute(const char *key)
        {
            return (*doc)["attributes"][key];
        }
    };
}