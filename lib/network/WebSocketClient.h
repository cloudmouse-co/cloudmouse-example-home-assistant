// lib/sdk/network/WebSocketClient.h
#pragma once

#include <Arduino.h>
#include <esp_websocket_client.h>
#include <functional>

namespace CloudMouse::SDK
{
    /**
     * @brief Callback invoked when WebSocket connection is established
     */
    using WsOnConnectedCallback = std::function<void()>;

    /**
     * @brief Callback invoked when WebSocket connection is lost
     */
    using WsOnDisconnectedCallback = std::function<void()>;

    /**
     * @brief Callback invoked when a text message is received
     * @param payload The message payload as string
     */
    using WsOnMessageCallback = std::function<void(const String& payload)>;

    /**
     * @brief Callback invoked when an error occurs
     * @param error Error description
     */
    using WsOnErrorCallback = std::function<void(const String& error)>;

    /**
     * @brief Native ESP32 WebSocket client using ESP-IDF
     * 
     * Uses ESP32's built-in esp_websocket_client for maximum reliability
     * and performance. No external library dependencies!
     * 
     * **Features:**
     * - Native ESP-IDF implementation
     * - Automatic reconnection
     * - Event-driven architecture
     * - Zero external dependencies
     * - Production-grade stability
     * 
     * **Usage Example:**
     * @code
     * auto ws = new WebSocketClient("ws://192.168.1.100:8123/api/websocket");
     * 
     * ws->setOnConnected([]() {
     *     Serial.println("Connected!");
     * });
     * 
     * ws->setOnMessage([](const String& msg) {
     *     Serial.printf("Received: %s\n", msg.c_str());
     * });
     * 
     * ws->begin();
     * @endcode
     * 
     * @note No loop() needed - fully event-driven!
     * @note Designed for ESP32, uses native ESP-IDF
     */
    class WebSocketClient
    {
    private:
        esp_websocket_client_handle_t client;  ///< Native ESP-IDF WebSocket handle
        String url;                             ///< WebSocket URL
        bool connected;                         ///< Connection state
        
        WsOnConnectedCallback onConnected;         ///< Connected callback
        WsOnDisconnectedCallback onDisconnected;   ///< Disconnected callback
        WsOnMessageCallback onMessage;             ///< Message callback
        WsOnErrorCallback onError;                 ///< Error callback

    public:
        /**
         * @brief Constructs a new WebSocket Client
         * 
         * @param url Full WebSocket URL (e.g., "ws://192.168.1.100:8123/api/websocket")
         */
        WebSocketClient(const String& url);

        /**
         * @brief Destructor - cleans up WebSocket client
         */
        ~WebSocketClient();

        /**
         * @brief Initializes and starts the WebSocket connection
         * 
         * @note Connection is asynchronous - use setOnConnected() callback
         */
        void begin();

        /**
         * @brief Gracefully closes the WebSocket connection
         */
        void disconnect();

        /**
         * @brief Checks current connection status
         * 
         * @return true if connected
         * @return false if disconnected
         */
        bool isConnected() const { return connected; }

        /**
         * @brief Sends a text message to the server
         * 
         * @param message Text message to send
         * @return true if sent successfully
         * @return false if not connected or send failed
         */
        bool sendText(const String& message);

        /**
         * @brief Sends binary data to the server
         * 
         * @param data Pointer to binary data
         * @param length Data length in bytes
         * @return true if sent successfully
         * @return false if not connected or send failed
         */
        bool sendBinary(const char* data, size_t length);

        /**
         * @brief Sets callback for connection established event
         * 
         * @param callback Function to invoke when connected
         */
        void setOnConnected(WsOnConnectedCallback callback) { onConnected = callback; }

        /**
         * @brief Sets callback for disconnection event
         * 
         * @param callback Function to invoke when disconnected
         */
        void setOnDisconnected(WsOnDisconnectedCallback callback) { onDisconnected = callback; }

        /**
         * @brief Sets callback for text message reception
         * 
         * @param callback Function to invoke with received text
         */
        void setOnMessage(WsOnMessageCallback callback) { onMessage = callback; }

        /**
         * @brief Sets callback for error events
         * 
         * @param callback Function to invoke on errors
         */
        void setOnError(WsOnErrorCallback callback) { onError = callback; }

    private:
        /**
         * @brief Static event handler for ESP-IDF WebSocket events
         * 
         * @param handler_args Pointer to WebSocketClient instance
         * @param base Event base
         * @param event_id Event ID
         * @param event_data Event data
         */
        static void websocket_event_handler(void* handler_args, esp_event_base_t base, int32_t event_id, void* event_data);
    };
}