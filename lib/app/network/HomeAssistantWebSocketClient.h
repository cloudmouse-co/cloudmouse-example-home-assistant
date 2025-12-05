// lib/app/services/HomeAssistantWebSocketClient.h
#pragma once

#include "../../network/WebSocketClient.h"
#include <ArduinoJson.h>
#include <functional>

namespace CloudMouse::App
{
    using OnHAConnectedCallback = std::function<void()>;
    using OnHAStateChangedCallback = std::function<void(const String& entityId, const String& stateJson)>;
    using OnHAErrorCallback = std::function<void(const String& error)>;

    /**
     * @brief Home Assistant WebSocket protocol handler
     * 
     * Handles HA authentication and state_changed events
     */
    class HomeAssistantWebSocketClient
    {
    private:
        CloudMouse::SDK::WebSocketClient* wsClient;
        String token;
        bool isAuthenticated;
        uint32_t messageId;

        OnHAConnectedCallback onConnected;
        OnHAStateChangedCallback onStateChanged;
        OnHAErrorCallback onError;

    public:
        HomeAssistantWebSocketClient(const String& host, const String &port, const String& token);
        ~HomeAssistantWebSocketClient();

        void begin();
        void disconnect();
        bool isConnected() const { return isAuthenticated; }

        void setOnConnected(OnHAConnectedCallback callback) { onConnected = callback; }
        void setOnStateChanged(OnHAStateChangedCallback callback) { onStateChanged = callback; }
        void setOnError(OnHAErrorCallback callback) { onError = callback; }

    private:
        void handleMessage(const String& payload);
        void authenticate();
        void subscribeToStateChanges();
        void handleStateChangeEvent(JsonDocument& doc);
    };
}