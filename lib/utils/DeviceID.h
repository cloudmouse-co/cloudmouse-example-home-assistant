/**
 * CloudMouse SDK - Device Identity Manager
 *
 * Provides device identification utilities using ESP32 hardware features.
 * Generates unique IDs, UUIDs (RFC 4122 v5 compliant), and Access Point credentials.
 *
 * Features:
 * - Deterministic device ID generation (MAC-based)
 * - RFC 4122 UUID v5 creation (SHA-1 based, persistent)
 * - mDNS hostname generation for local web server
 * - Access Point SSID/password generation
 * - Device information logging
 */

#ifndef DEVICEID_H
#define DEVICEID_H

#include <Arduino.h>
#include <esp_system.h>
#include <mbedtls/sha1.h>

namespace CloudMouse::Utils
{
    class DeviceID
    {
    private:
        // CloudMouse namespace UUID for UUID v5 generation
        // This creates a unique namespace for all CloudMouse devices
        // Generated once, hardcoded for consistency
        static constexpr uint8_t CLOUDMOUSE_NAMESPACE[16] = {
            0x6b, 0xa7, 0xb8, 0x10, 0x9d, 0xad, 0x11, 0xd1,
            0x80, 0xb4, 0x00, 0xc0, 0x4f, 0xd4, 0x30, 0xc8
        };

    public:
        // Get unique ESP32 device ID (last 4 bytes of MAC address)
        // Returns: 8-character hex string (e.g., "12a3f4e2")
        // Use: Human-readable identification, debugging, logs
        static String getDeviceID()
        {
            uint64_t chipid = ESP.getEfuseMac();
            uint32_t low = (uint32_t)chipid;

            char id[9];
            snprintf(id, sizeof(id), "%08x", low);

            return String(id);
        }

        // Generate RFC 4122 compliant UUID v5 (deterministic, SHA-1 based)
        // Returns: Standard UUID format (e.g., "6ba7b810-9dad-51d1-80b4-00c04fd430c8")
        // Use: Cloud authentication, database primary key, WebSocket authorization
        // Note: Same device always generates same UUID (deterministic)
        static String getDeviceUUID()
        {
            uint64_t mac = ESP.getEfuseMac();
            uint8_t macBytes[6];
            memcpy(macBytes, &mac, 6);

            // Prepare data for hashing: namespace + MAC address
            uint8_t data[16 + 6];
            memcpy(data, CLOUDMOUSE_NAMESPACE, 16);
            memcpy(data + 16, macBytes, 6);

            // Compute SHA-1 hash
            uint8_t hash[20];
            mbedtls_sha1(data, sizeof(data), hash);

            // Format as UUID v5 (use first 16 bytes of hash)
            // Set version (4 bits) to 5 and variant (2 bits) to RFC 4122
            hash[6] = (hash[6] & 0x0F) | 0x50; // Version 5
            hash[8] = (hash[8] & 0x3F) | 0x80; // Variant RFC 4122

            // Format as UUID string
            char uuid[37];
            snprintf(uuid, sizeof(uuid),
                     "%02x%02x%02x%02x-%02x%02x-%02x%02x-%02x%02x-%02x%02x%02x%02x%02x%02x",
                     hash[0], hash[1], hash[2], hash[3],
                     hash[4], hash[5],
                     hash[6], hash[7],
                     hash[8], hash[9],
                     hash[10], hash[11], hash[12], hash[13], hash[14], hash[15]);

            return String(uuid);
        }

        // Generate mDNS hostname for local web server access
        // Returns: hostname without .local suffix (e.g., "cm-12a3f4e2")
        // Use: MDNS.begin(getMDNSHostname().c_str())
        // Access: http://cm-12a3f4e2.local
        static String getMDNSHostname()
        {
            return "cm-" + getDeviceID();
        }

        // Generate Access Point SSID
        // Returns: SSID string (e.g., "CloudMouse-12a3f4e2")
        static String getAPSSID()
        {
            return "CloudMouse-" + getDeviceID();
        }

        // Generate simple AP password (first 8 characters of device ID)
        // Returns: 8-character hex string
        // Note: Use getAPPasswordSecure() for production
        static String getAPPassword()
        {
            String id = getDeviceID();
            return id.substring(0, 8);
        }

        // Generate secure AP password with MAC byte mixing
        // Returns: 10-character hex string with XOR mixing
        // Recommended for production use
        static String getAPPasswordSecure()
        {
            uint64_t mac = ESP.getEfuseMac();
            uint8_t *macBytes = (uint8_t *)&mac;

            char pass[11];
            snprintf(pass, sizeof(pass), "%02x%02x%02x%02x%02x",
                     macBytes[0] ^ macBytes[3],
                     macBytes[1] ^ macBytes[4],
                     macBytes[2] ^ macBytes[5],
                     macBytes[3] ^ macBytes[0],
                     macBytes[4] ^ macBytes[1]);

            return String(pass);
        }

        // Get formatted MAC address
        // Returns: Standard MAC format (e.g., "AA:BB:CC:DD:EE:FF")
        static String getMACAddress()
        {
            uint64_t mac = ESP.getEfuseMac();
            uint8_t *macBytes = (uint8_t *)&mac;

            char macStr[18];
            snprintf(macStr, sizeof(macStr), "%02X:%02X:%02X:%02X:%02X:%02X",
                     macBytes[0], macBytes[1], macBytes[2],
                     macBytes[3], macBytes[4], macBytes[5]);

            return String(macStr);
        }

        // Print comprehensive device information to Serial
        // Use: Debugging, initial device setup, support diagnostics
        static void printDeviceInfo()
        {
            Serial.println("\n========================================");
            Serial.println("    CloudMouse Device Information");
            Serial.println("========================================");
            Serial.printf("Device ID:       %s\n", getDeviceID().c_str());
            Serial.printf("Device UUID:     %s\n", getDeviceUUID().c_str());
            Serial.printf("MAC Address:     %s\n", getMACAddress().c_str());
            Serial.printf("mDNS Hostname:   %s.local\n", getMDNSHostname().c_str());
            Serial.println("----------------------------------------");
            Serial.printf("AP SSID:         %s\n", getAPSSID().c_str());
            Serial.printf("AP Password:     %s\n", getAPPassword().c_str());
            Serial.printf("AP Pass (Secure): %s\n", getAPPasswordSecure().c_str());
            Serial.println("----------------------------------------");
            Serial.printf("Chip Model:      %s\n", ESP.getChipModel());
            Serial.printf("Chip Revision:   %d\n", ESP.getChipRevision());
            Serial.printf("CPU Frequency:   %d MHz\n", ESP.getCpuFreqMHz());
            Serial.printf("Flash Size:      %d MB\n", ESP.getFlashChipSize() / (1024 * 1024));
            Serial.printf("Free Heap:       %d KB\n", ESP.getFreeHeap() / 1024);
            Serial.println("========================================\n");
        }
    };
}

#endif