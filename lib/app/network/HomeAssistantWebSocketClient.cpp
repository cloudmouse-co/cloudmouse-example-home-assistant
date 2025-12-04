// lib/app/services/HomeAssistantWebSocketClient.cpp

#include "HomeAssistantWebSocketClient.h"
#include "../../utils/Logger.h"
#include "../utils/HomeAssistantUtils.h"

namespace CloudMouse::App
{
    HomeAssistantWebSocketClient::HomeAssistantWebSocketClient(
        const String& host, 
        uint16_t port, 
        const String& token
    ) : token(token), isAuthenticated(false), messageId(1)
    {
        String url = "ws://" + host + ":" + String(port) + "/api/websocket";
        wsClient = new CloudMouse::SDK::WebSocketClient(url);
    }

    HomeAssistantWebSocketClient::~HomeAssistantWebSocketClient()
    {
        delete wsClient;
    }

    void HomeAssistantWebSocketClient::begin()
    {
        APP_LOGGER("Starting HA WebSocket client");

        wsClient->setOnConnected([this]() {
            APP_LOGGER("WebSocket connected, waiting for auth_required");
            delay(100);  // Small delay
            authenticate();
        });

        wsClient->setOnMessage([this](const String& payload) {
            // APP_LOGGER("WEB SOCKET MESSAGE: %s", payload.c_str());
            handleMessage(payload);
        });

        wsClient->setOnDisconnected([this]() {
            APP_LOGGER("WebSocket disconnected");
            isAuthenticated = false;
        });

        wsClient->setOnError([this](const String& error) {
            APP_LOGGER("WebSocket error: %s", error.c_str());
            if (onError) {
                onError(error);
            }
        });

        wsClient->begin();
    }

    void HomeAssistantWebSocketClient::disconnect()
    {
        wsClient->disconnect();
        isAuthenticated = false;
    }

    void HomeAssistantWebSocketClient::handleMessage(const String& payload)
    {
        JsonDocument doc;
        DeserializationError error = deserializeJson(doc, payload);

        if (error) {
            APP_LOGGER("Failed to parse message: %s", error.c_str());
            return;
        }

        String type = doc["type"].as<String>();
        APP_LOGGER("HA message type: %s", type.c_str());

        if (type == "auth_required") {
            authenticate();
        }
        else if (type == "auth_ok") {
            APP_LOGGER("Authenticated successfully");
            isAuthenticated = true;
            subscribeToStateChanges();
            if (onConnected) {
                onConnected();
            }
        }
        else if (type == "auth_invalid") {
            APP_LOGGER("Authentication failed");
            if (onError) {
                onError("Authentication failed");
            }
        }
        else if (type == "event") {
            handleStateChangeEvent(doc);
        }
    }

    void HomeAssistantWebSocketClient::authenticate()
    {
        JsonDocument doc;
        doc["type"] = "auth";
        doc["access_token"] = token;

        String msg;
        serializeJson(doc, msg);
        
        APP_LOGGER("Authenticating...");
        wsClient->sendText(msg);
    }

    void HomeAssistantWebSocketClient::subscribeToStateChanges()
    {
        JsonDocument doc;
        doc["id"] = messageId++;
        doc["type"] = "subscribe_events";
        doc["event_type"] = "state_changed";

        String msg;
        serializeJson(doc, msg);

        APP_LOGGER("Subscribing to state_changed");
        wsClient->sendText(msg);
    }

    void HomeAssistantWebSocketClient::handleStateChangeEvent(JsonDocument& doc)
    {
        JsonObject eventData = doc["event"]["data"];
        String entityId = eventData["entity_id"].as<String>();

        if (!isValidEntity(entityId)) {
            return;
        }

        JsonObject newState = eventData["new_state"];

        if (newState.isNull()) {
            return;
        }

        String stateJson;
        serializeJson(newState, stateJson);

        APP_LOGGER("State changed: %s", entityId.c_str());
        APP_LOGGER("PAYLOAD: %s", stateJson.c_str());

        if (onStateChanged) {
            onStateChanged(entityId, stateJson);
        }
    }
}