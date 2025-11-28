

#pragma once

#include "../core/Core.h"
#include "./services/HomeAssistantDataService.h"
#include "./services/HomeAssistantPrefs.h"
#include "./network/HomeAssistantConfigServer.h"
#include "./ui/HomeAssistantDisplayManager.h"

namespace CloudMouse::App
{

    using namespace CloudMouse::App::Services;
    using namespace CloudMouse::App::Network;
    using namespace CloudMouse::App::Ui;

    enum class AppState
    {
        INITIALIZING,
        WIFI_READY,
        WIFI_LOST,
        SETUP_NEEDED,
        CONFIG_NEEDED,
        READY,
        ERROR
    };

    enum class AppEventType
    {
        SETUP_NEEDED = 0,
        SETUP_SET = 1,
        CONFIG_NEEDED = 2,
        CONFIG_SET = 3,
        DISPLAY_BOOTSTRAP = 4,

        SHOW_CONFIG_NEEDED = 10,
        SHOW_ENTITY_LIST = 11,
        SHOW_ENTITY_DETAIL = 12,
        SHOW_LOADING = 13,
        SHOW_ERROR = 14,

        WEBSOCKET_RECEIVED = 20,
        HTTP_API_SUCCESS = 21,
        HTTP_API_ERROR = 22,

        ENCODER_ROTATION = 30,
        ENCODER_CLICK = 31,
        ENCODER_LONG_PRESS = 32,
    };

    struct AppEventData
    {
        AppEventType type;
        uint32_t value;
        char stringData[128];

        AppEventData() : type(AppEventType::CONFIG_NEEDED), value(0)
        {
            stringData[0] = '\0';
        }

        static AppEventData event(AppEventType type)
        {
            AppEventData evt;
            evt.type = type;
            return evt;
        }

        static AppEventData apiError(const char *message, int errorCode)
        {
            AppEventData evt = AppEventData::event(AppEventType::HTTP_API_ERROR);
            strncpy(evt.stringData, message, sizeof(evt.stringData) - 1);
            evt.value = errorCode;
            return evt;
        }
    };

    // Helper to convert AppEventType to SDK Event with offset
    inline CloudMouse::Event toSDKEvent(const AppEventData &appEvent)
    {
        CloudMouse::Event sdkEvent;
        sdkEvent.type = static_cast<CloudMouse::EventType>(
            static_cast<int>(appEvent.type) + 100);
        sdkEvent.value = appEvent.value;
        strncpy(sdkEvent.stringData, appEvent.stringData, sizeof(sdkEvent.stringData) - 1);
        return sdkEvent;
    }

    // Helper to check if SDK event is actually a App event
    inline bool isAppEvent(const CloudMouse::Event &sdkEvent)
    {
        return static_cast<int>(sdkEvent.type) >= 100;
    }

    // Helper to convert SDK Event back to AppEventData
    inline AppEventData toAppEvent(const CloudMouse::Event &sdkEvent)
    {
        AppEventData appEvent;
        appEvent.type = static_cast<AppEventType>(
            static_cast<int>(sdkEvent.type) - 100);
        appEvent.value = sdkEvent.value;
        strncpy(appEvent.stringData, sdkEvent.stringData, sizeof(appEvent.stringData) - 1);
        return appEvent;
    }

    class HomeAssistantApp : public CloudMouse::IAppOrchestrator
    {
    public:
        HomeAssistantApp();
        ~HomeAssistantApp();

        bool initialize() override;
        void update() override;
        void processSDKEvent(const CloudMouse::Event &event) override;

    private:
        // Components references
        HomeAssistantDataService *dataService;
        HomeAssistantConfigServer *configServer;
        HomeAssistantPrefs *prefs;
        HomeAssistantDisplayManager *display;

        // State management
        AppState currentState;
        AppState previousState;

        void processAppEvent(const AppEventData &event);

        void changeState(AppState newState);
        void handleStateChange();

        void handleWiFiConnected();

        void notifyDisplay(const AppEventData &eventData);
        void onConfigurationSaved();
    };
} // namespace CloudMouse::App