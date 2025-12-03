/**
 * @file DeviceID.h
 * @brief CloudMouse SDK - Device Identity Manager
 * 
 * Provides device identification utilities using ESP32 hardware features.
 * Generates unique IDs, RFC 4122 compliant UUIDs, mDNS hostnames, and Access Point credentials.
 * 
 * @author CloudMouse Team
 * @date 2024
 * @version 2.0.0
 * 
 * @section features Features
 * - Deterministic device ID generation (MAC-based)
 * - RFC 4122 UUID v5 creation (SHA-1 based, persistent)
 * - mDNS hostname generation for local web server
 * - Access Point SSID/password generation
 * - Device information logging
 * 
 * @section usage Usage Example
 * @code
 * #include "DeviceID.h"
 * 
 * void setup() {
 *     Serial.begin(115200);
 *     
 *     // Print all device information
 *     CloudMouse::Utils::DeviceID::printDeviceInfo();
 *     
 *     // Get UUID for cloud authentication
 *     String uuid = CloudMouse::Utils::DeviceID::getDeviceUUID();
 *     
 *     // Setup mDNS for local web server
 *     MDNS.begin(CloudMouse::Utils::DeviceID::getMDNSHostname().c_str());
 * }
 * @endcode
 */

#ifndef DEVICEID_H
#define DEVICEID_H

#include <Arduino.h>
#include <esp_system.h>
#include <mbedtls/sha1.h>

namespace CloudMouse
{
    namespace Utils
    {
        /**
         * @class DeviceID
         * @brief Device identification and credential management utility
         * 
         * This class provides static methods for generating various device identifiers
         * and credentials based on the ESP32 hardware MAC address. All identifiers
         * are deterministic, meaning the same device will always generate the same values.
         * 
         * @section identity_types Identity Types
         * 
         * The class generates three types of identifiers for different purposes:
         * 
         * 1. **Device ID** (getDeviceID())
         *    - Format: 8-character hexadecimal string (e.g., "12a3f4e2")
         *    - Source: Last 4 bytes of MAC address
         *    - Use: Human-readable identification, debugging, logs
         * 
         * 2. **Device UUID** (getDeviceUUID())
         *    - Format: RFC 4122 compliant UUID v5 (e.g., "6ba7b810-9dad-51d1-80b4-00c04fd430c8")
         *    - Source: SHA-1 hash of (namespace + MAC address)
         *    - Use: Cloud authentication, database primary key, WebSocket authorization
         * 
         * 3. **mDNS Hostname** (getMDNSHostname())
         *    - Format: "cm-{DeviceID}" (e.g., "cm-12a3f4e2")
         *    - Use: Local network web server access (http://cm-12a3f4e2.local)
         * 
         * @section thread_safety Thread Safety
         * All methods are static and thread-safe as they only read from hardware.
         * 
         * @note This class is header-only and requires no separate implementation file.
         */
        class DeviceID
        {
        private:
            /**
             * @brief Get CloudMouse namespace UUID for UUID v5 generation
             * 
             * Returns a static array containing the CloudMouse namespace UUID.
             * This namespace ensures all CloudMouse devices generate UUIDs
             * within a consistent namespace, preventing collisions with other systems.
             * 
             * The namespace UUID used is based on the DNS namespace standard (RFC 4122).
             * 
             * @return Pointer to 16-byte namespace UUID array
             * 
             * @private
             */
            static const uint8_t* getNamespace()
            {
                static const uint8_t CLOUDMOUSE_NAMESPACE[16] = {
                    0x6b, 0xa7, 0xb8, 0x10, 0x9d, 0xad, 0x11, 0xd1,
                    0x80, 0xb4, 0x00, 0xc0, 0x4f, 0xd4, 0x30, 0xc8
                };
                return CLOUDMOUSE_NAMESPACE;
            }

        public:
            /**
             * @brief Get unique ESP32 device ID
             * 
             * Generates an 8-character hexadecimal string based on the last 4 bytes
             * of the device's MAC address (eFuse). This ID is deterministic and unique
             * per device.
             * 
             * @return String containing 8-character hex device ID (e.g., "12a3f4e2")
             * 
             * @note This value never changes for the same hardware device
             * @note Uses ESP.getEfuseMac() which reads from hardware eFuse
             * 
             * @see getMACAddress() for full MAC address
             * @see getDeviceUUID() for RFC 4122 compliant UUID
             */
            static String getDeviceID()
            {
                uint64_t chipid = ESP.getEfuseMac();
                uint32_t low = (uint32_t)chipid;

                char id[9];
                snprintf(id, sizeof(id), "%08x", low);

                return String(id);
            }

            /**
             * @brief Generate RFC 4122 compliant UUID v5
             * 
             * Creates a deterministic UUID v5 by computing SHA-1 hash of the
             * CloudMouse namespace UUID concatenated with the device's MAC address.
             * 
             * The UUID format follows RFC 4122:
             * - Version bits (4 bits) set to 5 (SHA-1 based)
             * - Variant bits (2 bits) set to RFC 4122 standard
             * 
             * This UUID is:
             * - **Persistent**: Same device always generates the same UUID
             * - **Unique**: SHA-1 hash ensures no collisions
             * - **Standard compliant**: Passes all RFC 4122 validators
             * - **Namespace isolated**: All CloudMouse devices share common namespace
             * 
             * @return String containing standard UUID format (36 characters including hyphens)
             *         Example: "6ba7b810-9dad-51d1-80b4-00c04fd430c8"
             * 
             * @note Use this UUID as the primary identifier for cloud services
             * @note This is the recommended identifier for database storage
             * @note Complies with RFC 4122 UUID v5 specification
             * 
             * @see getDeviceID() for shorter human-readable ID
             * @see https://tools.ietf.org/html/rfc4122 RFC 4122 Specification
             */
            static String getDeviceUUID()
            {
                uint64_t mac = ESP.getEfuseMac();
                uint8_t macBytes[6];
                memcpy(macBytes, &mac, 6);

                // Prepare data for hashing: namespace + MAC address
                uint8_t data[16 + 6];
                memcpy(data, getNamespace(), 16);
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

            /**
             * @brief Generate mDNS hostname for local web server
             * 
             * Creates a hostname suitable for mDNS (Multicast DNS) service discovery
             * on local networks. The format is "cm-{DeviceID}" where DeviceID is
             * the 8-character hardware identifier.
             * 
             * When used with mDNS, the device becomes accessible at:
             * http://cm-12a3f4e2.local
             * 
             * @return String containing hostname without .local suffix (e.g., "cm-12a3f4e2")
             * 
             * @note Use with MDNS.begin(getMDNSHostname().c_str())
             * @note The .local suffix is automatically added by mDNS protocol
             * @note Hostname is unique per device (MAC-based)
             * 
             * @par Example:
             * @code
             * MDNS.begin(DeviceID::getMDNSHostname().c_str());
             * Serial.printf("Access web UI at: http://%s.local\n", 
             *               DeviceID::getMDNSHostname().c_str());
             * @endcode
             * 
             * @see getDeviceID() for the underlying identifier
             */
            static String getMDNSHostname()
            {
                return "cm-" + getDeviceID();
            }

            /**
             * @brief Generate Access Point SSID
             * 
             * Creates a unique SSID for the device when operating in Access Point mode.
             * Format: "CloudMouse-{DeviceID}"
             * 
             * @return String containing AP SSID (e.g., "CloudMouse-12a3f4e2")
             * 
             * @note SSID is visible to users scanning for WiFi networks
             * @note Maximum length is within WiFi SSID limits (32 characters)
             * 
             * @see getAPPassword() for corresponding password
             * @see getAPPasswordSecure() for more secure password option
             */
            static String getAPSSID()
            {
                return "CloudMouse-" + getDeviceID();
            }

            /**
             * @brief Generate simple Access Point password
             * 
             * Creates a basic password using the first 8 characters of the device ID.
             * This provides minimal security and should only be used for development
             * or non-critical applications.
             * 
             * @return String containing 8-character hexadecimal password
             * 
             * @warning This password has low entropy (32 bits) and can be brute-forced
             * @note For production use, prefer getAPPasswordSecure()
             * 
             * @see getAPPasswordSecure() for enhanced security
             * @see getAPSSID() for corresponding SSID
             */
            static String getAPPassword()
            {
                String id = getDeviceID();
                return id.substring(0, 8);
            }

            /**
             * @brief Generate secure Access Point password
             * 
             * Creates an enhanced password by XOR-mixing MAC address bytes.
             * This provides better security than the simple password by introducing
             * byte mixing that increases entropy and makes pattern prediction harder.
             * 
             * The mixing algorithm:
             * - XORs complementary MAC bytes (byte[0] ^ byte[3], etc.)
             * - Creates 10-character hexadecimal output
             * - Deterministic but harder to predict than simple substring
             * 
             * @return String containing 10-character hexadecimal password
             * 
             * @note Recommended for production Access Point deployments
             * @note Still deterministic (same device = same password)
             * @note Provides ~40 bits of entropy
             * 
             * @see getAPPassword() for simpler but less secure alternative
             * @see getAPSSID() for corresponding SSID
             */
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

            /**
             * @brief Get formatted MAC address
             * 
             * Returns the device's MAC address in standard colon-separated format.
             * The MAC address is read from the ESP32's eFuse and represents the
             * device's unique hardware identifier.
             * 
             * @return String containing MAC address in format "AA:BB:CC:DD:EE:FF"
             * 
             * @note MAC address is uppercase hexadecimal
             * @note This is the raw hardware identifier used to derive other IDs
             * @note MAC address never changes for the same hardware
             * 
             * @see getDeviceID() for derived short identifier
             * @see getDeviceUUID() for UUID derived from MAC
             */
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

            /**
             * @brief Print comprehensive device information to Serial
             * 
             * Outputs a formatted table containing all device identifiers and
             * system information to the Serial port. Useful for debugging,
             * initial device setup, and support diagnostics.
             * 
             * Output includes:
             * - Device ID (short identifier)
             * - Device UUID (RFC 4122 compliant)
             * - MAC Address (hardware identifier)
             * - mDNS Hostname (local network access)
             * - Access Point credentials (SSID and passwords)
             * - Chip information (model, revision, frequency)
             * - Memory information (flash size, free heap)
             * 
             * @note Requires Serial.begin() to be called before use
             * @note Output is formatted for readability with ASCII table borders
             * 
             * @par Example Output:
             * @code
             * ========================================
             *     CloudMouse Device Information
             * ========================================
             * Device ID:       12a3f4e2
             * Device UUID:     6ba7b810-9dad-51d1-80b4-00c04fd430c8
             * MAC Address:     AA:BB:CC:DD:EE:FF
             * mDNS Hostname:   cm-12a3f4e2.local
             * ----------------------------------------
             * AP SSID:         CloudMouse-12a3f4e2
             * AP Password:     12a3f4e2
             * AP Pass (Secure): a1b2c3d4e5
             * ----------------------------------------
             * Chip Model:      ESP32-S3
             * Chip Revision:   0
             * CPU Frequency:   240 MHz
             * Flash Size:      8 MB
             * Free Heap:       256 KB
             * ========================================
             * @endcode
             * 
             * @see All other getter methods for individual values
             */
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
}

#endif // DEVICEID_H