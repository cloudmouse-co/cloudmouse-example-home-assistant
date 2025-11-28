#include "./HomeAssistantPrefs.h"
#include "../utils/Logger.h"

namespace CloudMouse::App::Services
{

    HomeAssistantPrefs::HomeAssistantPrefs() : cachedApiKey(""), cachedHost(""), cacheValid(false) {}

    bool HomeAssistantPrefs::init()
    {
        APP_LOGGER("💾 Initializing Prefs...");

        prefs.init();

        // Load initial data into cache
        cachedApiKey = prefs.getString(API_KEY_NVS_KEY);
        cachedHost = prefs.getString(HOST_NVS_KEY);

        cacheValid = true;

        APP_LOGGER("✅ Prefs initialized (API key: %s, host: %s)\n",
                   cachedApiKey.isEmpty() ? "NOT SET" : "SET",
                   cachedHost.c_str());

        return true;
    }

    // ============================================================================
    // API KEY and HOST MANAGEMENT
    // ============================================================================

    void HomeAssistantPrefs::setApiKey(const String &apiKey)
    {
        prefs.save(API_KEY_NVS_KEY, apiKey);
        cachedApiKey = apiKey;

        APP_LOGGER("💾 API key saved: %s", apiKey.isEmpty() ? "EMPTY" : "SET");
    }

    void HomeAssistantPrefs::setHost(const String &host)
    {
        prefs.save(HOST_NVS_KEY, host);
        cachedHost = host;

        APP_LOGGER("💾 HOST saved: %s", host.c_str());
    }

    String HomeAssistantPrefs::getApiKey()
    {
        if (cacheValid && !cachedApiKey.isEmpty())
        {
            return cachedApiKey;
        }

        cachedApiKey = prefs.get(API_KEY_NVS_KEY);
        return cachedApiKey;
    }

    String HomeAssistantPrefs::getHost()
    {
        if (cacheValid && !cachedHost.isEmpty())
        {
            return cachedHost;
        }

        cachedHost = prefs.get(HOST_NVS_KEY);
        return cachedHost;
    }

    bool HomeAssistantPrefs::hasApiKey()
    {
        return !getApiKey().isEmpty();
    }

    bool HomeAssistantPrefs::hasHost()
    {
        return !getHost().isEmpty();
    }

    void HomeAssistantPrefs::setSelectedEntities(const String &entitiesJson)
    {
        prefs.save(ENTITIES_NVS_KEY, entitiesJson);
        cachedEntities = entitiesJson;

        APP_LOGGER("💾 Selected entities saved (%d chars)", entitiesJson.length());
    }

    String HomeAssistantPrefs::getSelectedEntities()
    {
        if (cacheValid && !cachedEntities.isEmpty())
        {
            return cachedEntities;
        }

        cachedEntities = prefs.get(ENTITIES_NVS_KEY);
        return cachedEntities;
    }

    bool HomeAssistantPrefs::hasSelectedEntities()
    {
        return !getSelectedEntities().isEmpty();
    }

    void HomeAssistantPrefs::resetConfiguration() {
        setApiKey("");
        setHost("");
        setSelectedEntities("");

        cachedEntities = "";
        cachedApiKey = "";
        cachedHost = "";
    }

} // namespace CloudMouse::App::Services