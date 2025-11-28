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
            changeState(AppState::READY);
        }

        if (currentState == AppState::READY)
        {
            dataService = new HomeAssistantDataService(*prefs);
            if (!dataService->init())
            {
                APP_LOGGER("❌ Failed to initialize data service");
                changeState(AppState::ERROR);
            }
            APP_LOGGER("✅ Data service initialized");

            notifyDisplay(AppEventData::event(AppEventType::SHOW_ENTITY_LIST));
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

        if ( ! configServer->hasValidSetup()) 
        {
            changeState(AppState::SETUP_NEEDED);
            return;
        }

        if ( ! configServer->hasValidConfig())
        {
            changeState(AppState::CONFIG_NEEDED);
            return;
        }

        if (currentState != AppState::READY) 
        {
            changeState(AppState::READY);
            return;
        }
        
        notifyDisplay(AppEventData::event(AppEventType::CONFIG_SET));
    }
}
