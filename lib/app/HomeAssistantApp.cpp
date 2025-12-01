#include "HomeAssistantApp.h"
#include "../utils/Logger.h"

namespace CloudMouse::App
{

    HomeAssistantApp::HomeAssistantApp()
        : prefs(nullptr), configServer(nullptr), display(nullptr), currentState(AppState::INITIALIZING), previousState(AppState::INITIALIZING)
    {
        APP_LOGGER("📊 App constructor");
    }

    HomeAssistantApp::~HomeAssistantApp()
    {
        // Clean up dynamically allocated services
        if (dataService)
            delete dataService;
        if (prefs)
            delete prefs;
        if (configServer)
            delete configServer;

        APP_LOGGER("📊 App destroyed");
    }

    bool HomeAssistantApp::initialize()
    {
        APP_LOGGER("Initializing app orchestrator");

        changeState(AppState::INITIALIZING);

        prefs = new HomeAssistantPrefs();
        if (!prefs->init())
        {
            APP_LOGGER("❌ Failed to initialize preferences");
            changeState(AppState::ERROR);
            return false;
        }
        APP_LOGGER("✅ Preferences loaded");

        configServer = new HomeAssistantConfigServer(*prefs);

        display = new HomeAssistantDisplayManager(*prefs);
        display->init();

        notifyDisplay(AppEventData::event(AppEventType::DISPLAY_BOOTSTRAP));
        return true;
    }

    void HomeAssistantApp::update()
    {
        if (configServer)
        {
            configServer->update();
        }
    }

    void HomeAssistantApp::processSDKEvent(const CloudMouse::Event &event)
    {

        APP_LOGGER("is app event: %s - for event %d", isAppEvent(event) ? "TRUE" : "FALSE", event.type);

        if (isAppEvent(event))
        {
            processAppEvent(toAppEvent(event));
        }

        switch (event.type)
        {
        case CloudMouse::EventType::WIFI_CONNECTED:
            changeState(AppState::WIFI_READY);
            break;

        case CloudMouse::EventType::WIFI_DISCONNECTED:
            changeState(AppState::WIFI_LOST);
            break;

        default:
            break;
        }
    }

    void HomeAssistantApp::processAppEvent(const AppEventData &event)
    {
        APP_LOGGER("processing APP event inside orchestrator: %d", event.type);
        switch (event.type)
        {
        case AppEventType::FETCH_ENTITY_STATUS:
            APP_LOGGER("Received FETCH_ENTITY_STATUS for entity: %s", event.getStringData());
            dataService->fetchEntityStatus(event.getStringData());
            break;

        case AppEventType::CALL_SWITCH_ON_SERVICE:
            APP_LOGGER("Received CALL_SWITCH_ON_SERVICE for entity: %s", event.getStringData().c_str());
            dataService->setSwitchOn(event.getStringData());
            break;

        case AppEventType::CALL_SWITCH_OFF_SERVICE:
            APP_LOGGER("Received CALL_SWITCH_OFF_SERVICE for entity: %s", event.getStringData().c_str());
            dataService->setSwitchOff(event.getStringData());
            break;
        }
    }

    void HomeAssistantApp::changeState(AppState newState)
    {
        if (currentState == newState)
            return;

        previousState = currentState;
        currentState = newState;

        APP_LOGGER("📊 State change: %d -> %d\n", (int)previousState, (int)currentState);

        handleStateChange();
    }

    void HomeAssistantApp::handleStateChange()
    {
        switch (currentState)
        {
        case AppState::INITIALIZING:
            APP_LOGGER("⏳ Initializing - waiting for WiFi");
            break;

        case AppState::WIFI_READY:
            APP_LOGGER("📡 WiFi ready - validating configuration");
            handleWiFiConnected();
            break;

        case AppState::SETUP_NEEDED:
        case AppState::CONFIG_NEEDED:
        {
            APP_LOGGER("⚠️ Configuration needed");
            AppEventData configNeeded = AppEventData::event(AppEventType::SHOW_CONFIG_NEEDED);
            strncpy(configNeeded.stringData, configServer->getConfigURL().c_str(), sizeof(configNeeded.stringData) - 1);
            notifyDisplay(configNeeded);
            break;
        }

        case AppState::READY:
            APP_LOGGER("✅ Ready - preparing to start polling");
            notifyDisplay(AppEventData::event(AppEventType::CONFIG_SET));
            break;

        case AppState::ERROR:
            APP_LOGGER("❌ Error state");
            notifyDisplay(AppEventData::apiError("App error", -1));
            break;

        default:
            break;
        }
    }

    void HomeAssistantApp::handleWiFiConnected()
    {
        if (configServer && !configServer->init())
        {
            APP_LOGGER("❌ Failed to initialize config server");
            changeState(AppState::ERROR);
        }
        configServer->setConfigChangedCallback([this]()
                                               { this->onConfigurationSaved(); });
        APP_LOGGER("✅ Config server initialized");

        if (!configServer->hasValidSetup())
        {
            changeState(AppState::SETUP_NEEDED);
        }
        else if (!configServer->hasValidConfig())
        {
            changeState(AppState::CONFIG_NEEDED);
        }
        else
        {
            dataService = new HomeAssistantDataService(*prefs);
            if (!dataService->init())
            {
                APP_LOGGER("❌ Failed to initialize data service");
                changeState(AppState::ERROR);
            }
            APP_LOGGER("✅ Data service initialized");

            String entitiesJson = prefs->getSelectedEntities();
            JsonDocument doc;
            DeserializationError error = deserializeJson(doc, entitiesJson);

            JsonArray entities = doc.as<JsonArray>();
            int entityCount = entities.size();
            for (int i = 0; i < entityCount; i++)
            {
                JsonObject entity = entities[i];
                String entityId = entity["entity_id"].as<String>();
                dataService->fetchEntityStatus(entityId);
                delay(100);
            }

            changeState(AppState::READY);
        }
    }

    void HomeAssistantApp::notifyDisplay(const AppEventData &eventData)
    {
        APP_LOGGER("Sending event from App Orchstrator->notifyDisplay()");
        CloudMouse::EventBus::instance().sendToUI(toSDKEvent(eventData));
    }

    void HomeAssistantApp::onConfigurationSaved()
    {
        APP_LOGGER("RECEIVED Config changed from config server callback");

        if (!configServer->hasValidSetup())
        {
            changeState(AppState::SETUP_NEEDED);
            return;
        }

        if (!configServer->hasValidConfig())
        {
            changeState(AppState::CONFIG_NEEDED);
            return;
        }

        if (currentState != AppState::READY)
        {
            String entitiesJson = prefs->getSelectedEntities();
            JsonDocument doc;
            DeserializationError error = deserializeJson(doc, entitiesJson);

            JsonArray entities = doc.as<JsonArray>();
            int entityCount = entities.size();
            for (int i = 0; i < entityCount; i++)
            {
                JsonObject entity = entities[i];
                String entityId = entity["entity_id"].as<String>();
                dataService->fetchEntityStatus(entityId);
                delay(100);
            }

            changeState(AppState::READY);
            return;
        }

        notifyDisplay(AppEventData::event(AppEventType::CONFIG_SET));
    }
}
