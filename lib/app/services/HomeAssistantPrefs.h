#pragma once

#include "../prefs/PreferencesManager.h"

namespace CloudMouse::App::Services
{

    using namespace CloudMouse::Prefs;

    class HomeAssistantPrefs
    {
    public:
        HomeAssistantPrefs();
        ~HomeAssistantPrefs() = default;

        bool init();

        void setApiKey(const String &apiKey);
        String getApiKey();

        bool hasApiKey();

        void setHost(const String &host);
        String getHost();

        bool hasHost();

        void setSelectedEntities(const String &entitiesJson);
        String getSelectedEntities();
        bool hasSelectedEntities();

        void resetConfiguration();

    private:
        PreferencesManager prefs;

        const char *API_KEY_NVS_KEY = "ha_api_key";
        const char *HOST_NVS_KEY = "ha_host";
        const char *ENTITIES_NVS_KEY = "ha_entities";

        mutable String cachedApiKey;
        mutable String cachedHost;
        String cachedEntities;
        mutable bool cacheValid;
    };
} // namespace CloudMouse::App::Services