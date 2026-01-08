#include "HomeAssistantApp.h"
#include "./network/HomeAssistantWebSocketClient.h"
#include "./model/HomeAssistantAppStore.h"
#include "../utils/Logger.h"
#include "../utils/NTPManager.h"
#include "utils/HomeAssistantUtils.h"

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

        case CloudMouse::EventType::ENCODER_PRESS_TIME:
            // APP_LOGGER("ENCODER PRESS TIME: %d", event.value);
            if (500 <= event.value && !notified)
            {
                notified = true;
                notifyDisplay(AppEventData::event(AppEventType::DISPLAY_UPLEVEL));
            }
            break;

        case CloudMouse::EventType::ENCODER_BUTTON_RELEASED:
            // APP_LOGGER("ENCODER BUTTON RELEASED TIME: %d", event.value);
            notified = false;
            break;

        default:
            break;
        }
    }

    void HomeAssistantApp::processAppEvent(const AppEventData &event)
    {
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

        case AppEventType::CALL_LIGHT_ON_SERVICE:
            APP_LOGGER("Received CALL_LIGHT_ON_SERVICE for entity: %s", event.getStringData().c_str());
            dataService->setLightOn(event.getStringData());
            break;

        case AppEventType::CALL_LIGHT_OFF_SERVICE:
            APP_LOGGER("Received CALL_LIGHT_OFF_SERVICE for entity: %s", event.getStringData().c_str());
            dataService->setLightOff(event.getStringData());
            break;

        case AppEventType::CALL_COVER_CLOSE_SERVICE:
            APP_LOGGER("Received CALL_COVER_CLOSE_SERVICE for entity: %s", event.getStringData().c_str());
            dataService->setCoverClose(event.getStringData());
            break;

        case AppEventType::CALL_COVER_OPEN_SERVICE:
            APP_LOGGER("Received CALL_COVER_OPEN_SERVICE for entity: %s", event.getStringData().c_str());
            dataService->setCoverOpen(event.getStringData());
            break;

        case AppEventType::CALL_COVER_STOP_SERVICE:
            APP_LOGGER("Received CALL_COVER_STOP_SERVICE for entity: %s", event.getStringData().c_str());
            dataService->setCoverStop(event.getStringData());
            break;

        case AppEventType::CALL_CLIMATE_SET_MODE:
        {
            APP_LOGGER("Received CALL_CLIMATE_SET_MODE for entity: %s", event.getStringData().c_str());
            char entity_id[64];
            char mode[32];

            sscanf(event.stringData, "%[^|]|%s", entity_id, mode);
            dataService->setClimateMode(String(entity_id), String(mode));
            break;
        }

        case AppEventType::CALL_CLIMATE_SET_TEMPERATURE:
        {
            APP_LOGGER("Received CALL_CLIMATE_SET_TEMPERATURE for entity: %s", event.getStringData().c_str());
            char entity_id[64];
            float temp;

            sscanf(event.stringData, "%[^|]|%f", entity_id, &temp);
            dataService->setClimateTemperature(String(entity_id), temp);
            break;
        }

        case AppEventType::CALL_ALL_LIGHTS_OFF:
            APP_LOGGER("Received CALL_ALL_LIGHTS_OFF for entity: %s", event.getStringData().c_str());
            dataService->setAllLightsOff();
            break;

        case AppEventType::CALL_ALL_COVERS_DOWN:
            APP_LOGGER("Received CALL_ALL_COVERS_DOWN for entity: %s", event.getStringData().c_str());
            dataService->setAllCoversDown();
            break;
            
        case AppEventType::CALL_ALL_SWITCH_OFF:
            APP_LOGGER("Received CALL_ALL_SWITCH_OFF for entity: %s", event.getStringData().c_str());
            dataService->setAllSwitchesOff();
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

        // CloudMouse::Utils::NTPManager::setTimezone(3600, 3600);

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

            notifyDisplay(AppEventData::event(AppEventType::SHOW_LOADING));

            wsClient = new HomeAssistantWebSocketClient(
                prefs->getHost(),
                prefs->getPort(),
                prefs->getApiKey());

            wsClient->setOnConnected([this]()
                                     { APP_LOGGER("HA WebSocket ready"); });

            wsClient->setOnStateChanged([this](const String &entityId, const String &stateJson)
                                        {
                    Core::instance().getLEDManager()->flashColor(153,23,80, 255, 200);
                    AppStore::instance().setEntity(entityId, stateJson);
                    EventBus::instance().sendToUI(
                        toSDKEvent(AppEventData::entityUpdated(entityId.c_str()))
                    ); });

            wsClient->begin();

            if (fetchSelectedEntities())
            {
                changeState(AppState::READY);
            }
            else
            {
                changeState(AppState::ERROR);
            }
        }
    }

    void HomeAssistantApp::notifyDisplay(const AppEventData &eventData)
    {
        CloudMouse::EventBus::instance().sendToUI(toSDKEvent(eventData));
    }

    void HomeAssistantApp::onConfigurationSaved()
    {
        APP_LOGGER("RECEIVED Config changed from config server callback");

        if (!configServer->hasValidSetup())
        {
            APP_LOGGER("SETUP not valid");
            changeState(AppState::SETUP_NEEDED);
            return;
        }

        if (!configServer->hasValidConfig())
        {
            APP_LOGGER("CONFIG not valid");
            changeState(AppState::CONFIG_NEEDED);
            return;
        }

        notifyDisplay(AppEventData::event(AppEventType::SHOW_LOADING));

        if (fetchSelectedEntities())
        {
            if (currentState != AppState::READY)
            {
                // this will notify a CONFIG_SET event to the display
                // called the first time the user set the config
                changeState(AppState::READY);
            }
            else
            {
                // otherwise we call config set even tho the systems
                // is already in READY state (called every time the config is updated)
                notifyDisplay(AppEventData::event(AppEventType::CONFIG_SET));
            }
        }
        else
        {
            changeState(AppState::ERROR);
        }
    }

    bool HomeAssistantApp::fetchSelectedEntities()
    {
        APP_LOGGER("FETCHING SELECTED ENTITIES");

        String entitiesJson = prefs->getSelectedEntities();
        JsonDocument doc;
        DeserializationError error = deserializeJson(doc, entitiesJson);

        if (error)
        {
            APP_LOGGER("DESERIALIZATION ERROR %s", error.c_str());
            return false;
        }

        JsonArray entities = doc.as<JsonArray>();
        int entityCount = entities.size();
        Core::instance().getLEDManager()->setLoadingState(true);
        for (int i = 0; i < entityCount; i++)
        {
            JsonObject entity = entities[i];
            String entityId = entity["entity_id"].as<String>();
            dataService->fetchEntityStatus(entityId);
            delay(50);
        }
        Core::instance().getLEDManager()->setLoadingState(false);

        return true;
    }
}
