// lib/sdk/network/WebSocketClient.cpp

#include "WebSocketClient.h"
#include "../utils/Logger.h"

namespace CloudMouse::SDK
{
    WebSocketClient::WebSocketClient(const String& url)
        : url(url), connected(false), client(nullptr)
    {
    }

    WebSocketClient::~WebSocketClient()
    {
        if (client) {
            esp_websocket_client_stop(client);
            esp_websocket_client_destroy(client);
        }
    }

    void WebSocketClient::begin()
    {
        SDK_LOGGER("Connecting WebSocket to %s", url.c_str());
        
        esp_websocket_client_config_t ws_cfg = {};
        ws_cfg.uri = url.c_str();
        
        client = esp_websocket_client_init(&ws_cfg);
        
        esp_websocket_register_events(client, WEBSOCKET_EVENT_ANY, websocket_event_handler, this);
        
        esp_websocket_client_start(client);
    }

    void WebSocketClient::disconnect()
    {
        SDK_LOGGER("Disconnecting WebSocket");
        if (client) {
            esp_websocket_client_stop(client);
        }
        connected = false;
    }

    bool WebSocketClient::sendText(const String& message)
    {
        if (!connected || !client) {
            SDK_LOGGER("Cannot send - not connected");
            return false;
        }
        
        int sent = esp_websocket_client_send_text(client, message.c_str(), message.length(), portMAX_DELAY);
        return sent >= 0;
    }

    bool WebSocketClient::sendBinary(const char* data, size_t length)
    {
        if (!connected || !client) {
            SDK_LOGGER("Cannot send binary - not connected");
            return false;
        }
        
        int sent = esp_websocket_client_send_bin(client, data, length, portMAX_DELAY);
        return sent >= 0;
    }

    void WebSocketClient::websocket_event_handler(void* handler_args, esp_event_base_t base, int32_t event_id, void* event_data)
    {
        WebSocketClient* self = static_cast<WebSocketClient*>(handler_args);
        esp_websocket_event_data_t* data = (esp_websocket_event_data_t*)event_data;

        switch (event_id) {
            case WEBSOCKET_EVENT_CONNECTED:
                SDK_LOGGER("WebSocket Connected");
                self->connected = true;
                if (self->onConnected) {
                    self->onConnected();
                }
                break;

            case WEBSOCKET_EVENT_DISCONNECTED:
                SDK_LOGGER("WebSocket Disconnected");
                self->connected = false;
                if (self->onDisconnected) {
                    self->onDisconnected();
                }
                break;

            case WEBSOCKET_EVENT_DATA:
                if (data->op_code == 0x01) {  // Text frame
                    String message = String((char*)data->data_ptr, data->data_len);
                    SDK_LOGGER("WebSocket received: %s", message.c_str());
                    if (self->onMessage) {
                        self->onMessage(message);
                    }
                }
                break;

            case WEBSOCKET_EVENT_ERROR:
                SDK_LOGGER("WebSocket Error");
                if (self->onError) {
                    self->onError("WebSocket error occurred");
                }
                break;

            default:
                break;
        }
    }
}