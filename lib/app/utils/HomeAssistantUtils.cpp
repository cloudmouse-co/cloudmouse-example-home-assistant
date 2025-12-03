#include "HomeAssistantUtils.h"

namespace CloudMouse::App {

    bool isValidEntity(const String &entityId)
    {
        if (entityId.startsWith("light.") ||
            entityId.startsWith("sensor.") ||
            entityId.startsWith("climate.") ||
            entityId.startsWith("switch.") ||
            entityId.startsWith("weather.") ||
            entityId.startsWith("cover."))
            return true;

        return false;
    }

}