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
        ws_cfg.buffer_size = 4096;  // Increase buffer
        ws_cfg.disable_auto_reconnect = false;
        ws_cfg.ping_interval_sec = 10;
        ws_cfg.task_stack = 4096;
        
        // Add user agent
        ws_cfg.user_agent = "ESP32-CloudMouse";
        
        client = esp_websocket_client_init(&ws_cfg);
        
        esp_websocket_register_events(client, WEBSOCKET_EVENT_ANY, websocket_event_handler, this);
        
        SDK_LOGGER("Starting WebSocket client...");
        esp_err_t err = esp_websocket_client_start(client);
        SDK_LOGGER("WebSocket start result: %d", err);
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
        SDK_LOGGER("Event handler called! event_id: %d", event_id);  
        
        WebSocketClient* self = static_cast<WebSocketClient*>(handler_args);
        esp_websocket_event_data_t* data = (esp_websocket_event_data_t*)event_data;

        switch (event_id) {
            case WEBSOCKET_EVENT_CONNECTED:
                SDK_LOGGER("WebSocket Connected");
                self->connected = true;
                
                // Check if there's data in this event
                if (data && data->data_len > 0) {
                    SDK_LOGGER("Data in CONNECTED event: %d bytes", data->data_len);
                    String message = String((char*)data->data_ptr, data->data_len);
                    SDK_LOGGER("Message: %s", message.c_str());
                    if (self->onMessage) {
                        self->onMessage(message);
                    }
                }
                
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
                SDK_LOGGER("WebSocket data received, op_code: 0x%02x, len: %d", data->op_code, data->data_len);
                
                if (data->op_code == 0x08) {  // CLOSE frame
                    // Read close code (first 2 bytes)
                    if (data->data_len >= 2) {
                        uint16_t close_code = (data->data_ptr[0] << 8) | data->data_ptr[1];
                        SDK_LOGGER("------------------------------------------------------------- WebSocket CLOSE code: %d", close_code);
                    }
                }

                // Handle both text (0x01) and continuation frames (0x00)
                if (data->op_code == 0x01 || data->op_code == 0x00) {
                    if (data->data_ptr && data->data_len > 0) {
                        String message = String((char*)data->data_ptr, data->data_len);
                        SDK_LOGGER("WebSocket message: %s", message.c_str());
                        if (self->onMessage) {
                            self->onMessage(message);
                        }
                    }
                }
                break;

            case WEBSOCKET_EVENT_ERROR:
                SDK_LOGGER("WebSocket Error");
                if (self->onError) {
                    self->onError("WebSocket error occurred");
                }
                break;

            case WEBSOCKET_EVENT_CLOSED:
                SDK_LOGGER("WebSocket Closed");
                break;


            default:
                SDK_LOGGER("WebSocket unknown event: %d", event_id);
                break;
        }
    }
}