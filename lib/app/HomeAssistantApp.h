

#pragma once

#include "../core/Core.h"
#include "./services/HomeAssistantDataService.h"
#include "./services/HomeAssistantPrefs.h"
#include "./network/HomeAssistantConfigServer.h"
#include "./ui/HomeAssistantDisplayManager.h"

namespace CloudMouse::App
{
    class HomeAssistantWebSocketClient;
}

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

        FETCH_ENTITY_STATUS = 40,
        CALL_SWITCH_ON_SERVICE = 41,
        CALL_SWITCH_OFF_SERVICE = 42,
        CALL_LIGHT_ON_SERVICE = 43,
        CALL_LIGHT_OFF_SERVICE = 44,
        ENTITY_UPDATED = 45,

        DISPLAY_UPLEVEL = 50,
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

        static AppEventData fetchEntityStatus(const char *entity_id)
        {
            AppEventData evt = AppEventData::event(AppEventType::FETCH_ENTITY_STATUS);
            evt.setStringData(entity_id);
            return evt;
        }

        static AppEventData entityUpdated(const String &entity_id)
        {
            AppEventData evt = AppEventData::event(AppEventType::ENTITY_UPDATED);
            evt.setStringData(entity_id);
            return evt;
        }

        static AppEventData callSwitchOn(const String &entity_id)
        {
            AppEventData evt = AppEventData::event(AppEventType::CALL_SWITCH_ON_SERVICE);
            evt.setStringData(entity_id);
            return evt;
        }

        static AppEventData callSwitchOff(const String &entity_id)
        {
            AppEventData evt = AppEventData::event(AppEventType::CALL_SWITCH_OFF_SERVICE);
            evt.setStringData(entity_id);
            return evt;
        }

        static AppEventData callLightOn(const String &entity_id)
        {
            AppEventData evt = AppEventData::event(AppEventType::CALL_LIGHT_ON_SERVICE);
            evt.setStringData(entity_id);
            return evt;
        }

        static AppEventData callLightOff(const String &entity_id)
        {
            AppEventData evt = AppEventData::event(AppEventType::CALL_LIGHT_OFF_SERVICE);
            evt.setStringData(entity_id);
            return evt;
        }
        /**
         * Set string payload with automatic truncation and null termination
         * Safely copies string data with bounds checking to prevent buffer overflow
         *
         * @param str String data to store (will be truncated if > 255 characters)
         */
        void setStringData(const String &str)
        {
            strncpy(stringData, str.c_str(), sizeof(stringData) - 1);
            stringData[sizeof(stringData) - 1] = '\0'; // Ensure null termination
        }

        /**
         * Get string payload as Arduino String object
         * Safe accessor that always returns valid string (empty if unset)
         *
         * @return String object containing current string data
         */
        String getStringData() const
        {
            return String(stringData);
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
        HomeAssistantWebSocketClient *wsClient;

        // State management
        AppState currentState;
        AppState previousState;

        bool notified = false;

        void processAppEvent(const AppEventData &event);

        void changeState(AppState newState);
        void handleStateChange();

        void handleWiFiConnected();

        void notifyDisplay(const AppEventData &eventData);
        void onConfigurationSaved();
    };
} // namespace CloudMouse::App