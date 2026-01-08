# CloudMouse Home Assistant App - Developer Documentation

## 📋 Overview

A complete Home Assistant integration for [CloudMouse IoT devices](https://cloudmouse.co), featuring real-time WebSocket updates, interactive LVGL UI, and comprehensive entity control across multiple device types.

![License](https://img.shields.io/badge/license-MIT-blue.svg)
![Platform](https://img.shields.io/badge/platform-ESP32-green.svg)
![LVGL](https://img.shields.io/badge/LVGL-v9.4-orange.svg)

> **Note**: This application requires a [CloudMouse device](https://cloudmouse.co) or compatible hardware with ESP32, ILI9488 display, rotary encoder, and RGB LED.

## 🏗️ Architecture

### Core Components
```
lib/app/
├── HomeAssistantApp.cpp/h          # Main orchestrator
├── model/
│   ├── HomeAssistantAppStore.h     # Thread-safe entity state management
│   └── HomeAssistantEntity.h       # Entity data model with PSRAM allocation
├── network/
│   ├── HomeAssistantConfigServer   # Web-based configuration interface
│   └── HomeAssistantWebSocketClient # Real-time HA WebSocket protocol
├── services/
│   ├── HomeAssistantDataService    # HTTP API calls & entity fetching
│   └── HomeAssistantPrefs          # NVS-backed configuration storage
├── ui/
│   └── HomeAssistantDisplayManager # LVGL-based UI rendering
└── utils/
    └── HomeAssistantUtils          # Entity validation helpers
```

## 🔄 State Machine
```
INITIALIZING → WIFI_READY → CONFIG_NEEDED → READY → RUNNING
                    ↓             ↓
                SETUP_NEEDED   ERROR
```

### State Transitions

- **INITIALIZING**: Boot sequence, waiting for WiFi
- **WIFI_READY**: WiFi connected, validating configuration
- **SETUP_NEEDED**: No HA credentials configured
- **CONFIG_NEEDED**: Credentials exist, no entities selected
- **READY**: All configured, preparing to start
- **RUNNING**: Active operation with WebSocket connection

## 🔌 Event System

### SDK Event Flow
```
Core (Core 0) ← EventBus → Display (Core 1)
       ↕                           ↕
   AppOrchestrator          DisplayManager
```

### Custom App Events

Events are offset by `+100` from SDK events to avoid conflicts:
```cpp
AppEventType::CONFIG_SET (3) → EventType (103)
```

**Key Events:**
- `SETUP_NEEDED/SET`: Configuration state changes
- `CONFIG_NEEDED/SET`: Entity selection state
- `ENTITY_UPDATED`: WebSocket state change received
- `FETCH_ENTITY_STATUS`: Request entity refresh
- `CALL_*_SERVICE`: Execute HA service calls

## 🗄️ Data Management

### AppStore (Thread-Safe State)
```cpp
// Core 0: Write from WebSocket
AppStore::instance().setEntity(entityId, payload);

// Core 1: Read in UI
auto entity = AppStore::instance().getEntity(entityId);
```

**Features:**
- Mutex-protected for dual-core safety
- PSRAM allocation for large JSON documents
- `std::shared_ptr` for safe memory management
- Automatic parsing and validation

### Entity Model
```cpp
class HomeAssistantEntity {
    const char* getEntityId();
    const char* getState();
    const char* getFriendlyName();
    JsonVariant getAttribute(const char* key);
}
```

## 🌐 Network Layer

### Configuration Server (Port 8080)

**Endpoints:**
- `GET /home-assistant` → Setup page (host/port/token)
- `POST /home-assistant/setup` → Save credentials
- `GET /home-assistant/config` → Entity selection page
- `POST /home-assistant/config/save` → Save selected entities

**mDNS:** `cloudmouse-{device_id}.local:8080`

### WebSocket Client

**Protocol Flow:**
1. Connect → `auth_required`
2. Authenticate with token → `auth_ok`
3. Subscribe to `state_changed` events
4. Receive real-time updates
```cpp
wsClient->setOnStateChanged([](const String& entityId, const String& stateJson) {
    AppStore::instance().setEntity(entityId, stateJson);
    EventBus::instance().sendToUI(
        toSDKEvent(AppEventData::entityUpdated(entityId))
    );
});
```

## 🎨 UI System (LVGL on Core 1)

### Screen Architecture
```
┌─────────────────────────────────────┐
│  Sidebar (70px)  │  Content (410px) │
│  ┌─────┐         │                  │
│  │ 🏠  │ ◄───────┤  Entity List     │
│  ├─────┤         │  or              │
│  │ 💡  │         │  Entity Detail   │
│  ├─────┤         │                  │
│  │ 🔌  │         │                  │
│  └─────┘         │                  │
└─────────────────────────────────────┘
```

### View Types

1. **LOADING**: Spinner during sync
2. **CONFIG_NEEDED**: QR code + setup URL
3. **ENTITY_LIST**: Filtered scrollable list
4. **ENTITY_DETAIL**: Type-specific controls

### Supported Entity Types

| Type     | Controls | Features |
|----------|----------|----------|
| `light.*` | ON/OFF buttons | LED status indicator |
| `switch.*` | ON/OFF buttons | LED status indicator |
| `climate.*` | Arc slider, ON/OFF | Temperature control (15-30°C) |
| `sensor.*` | Read-only display | Value + unit |
| `cover.*` | OPEN/STOP/CLOSE | Three-button control |

### Filtering System
```cpp
enum class EntityFilter {
    ALL, LIGHT, SWITCH, CLIMA, COVER, SENSOR
};
```

Each filter uses unique colors and Font Awesome icons.

## 🔧 Configuration Storage

### NVS Keys
```cpp
"ha_api_key"    // Home Assistant long-lived token
"ha_host"       // HA hostname/IP
"ha_port"       // HA port (default: 8123)
"ha_entities"   // JSON array of selected entities
```

### Entity Storage Format
```json
[
  {
    "entity_id": "light.living_room",
    "friendly_name": "Living Room Light",
    "state": "on"
  }
]
```

## 🎮 Encoder Interactions

| Action | List View | Detail View |
|--------|-----------|-------------|
| **Rotate** | Navigate items | Adjust values (climate) |
| **Click** | Toggle light/switch | Activate button |
| **Double-Click** | Open detail | - |
| **Long Press** | Focus sidebar | Back to list |

### Climate Arc Behavior

1. Click arc → Enter edit mode (color changes)
2. Rotate → Adjust temperature (0.1°C steps)
3. Click again → Save + send to HA

## 🚀 Service Calls
```cpp
// Switch
dataService->setSwitchOn("switch.garage");
dataService->setSwitchOff("switch.garage");

// Light
dataService->setLightOn("light.bedroom");
dataService->setLightOff("light.bedroom");

// Climate
dataService->setClimateTemperature("climate.living_room", 22.5);
dataService->setClimateMode("climate.living_room", "heat");

// Cover
dataService->setCoverOpen("cover.blinds");
dataService->setCoverStop("cover.blinds");
dataService->setCoverClose("cover.blinds");
```

## 🐛 Debugging

### Logger Macros
```cpp
APP_LOGGER("State change: %d -> %d", prev, current);
```

### Key Debug Points

1. **State transitions**: Watch `changeState()` calls
2. **WebSocket messages**: Monitor `handleMessage()`
3. **Entity updates**: Track `AppStore::setEntity()`
4. **UI events**: Check `processAppEvent()`

### Common Issues

**WebSocket not connecting:**
- Verify host/port/token in config
- Check HA allows WebSocket connections
- Monitor `HomeAssistantWebSocketClient` logs

**Entities not updating:**
- Confirm WebSocket subscription successful
- Check entity IDs in `isValidEntity()`
- Verify AppStore mutex not deadlocked

**UI not responding:**
- Check Core 1 task is running
- Verify encoder group has focused objects
- Monitor LVGL memory usage

## 📦 Dependencies

- **CloudMouse SDK**: Core system, EventBus, hardware managers
- **ArduinoJson**: Entity parsing and API responses
- **LVGL 9.4**: UI rendering and input handling
- **ESP32 Arduino**: WebSocket, HTTP client, NVS

## 🔐 Security Notes

- **Long-lived tokens** are stored in NVS (plaintext)
- **WebSocket authentication** uses bearer token
- **No encryption** on local network (standard HA setup)
- **mDNS** exposes device on local network

## 📝 TODO/Roadmap

- [ ] SSL/TLS support for WebSocket
- [ ] Scene activation support
- [ ] Automation triggering
- [ ] Cover position control (0-100%)
- [ ] Light brightness/color control
- [ ] Fan speed control
- [ ] Media player controls

## 🤝 Integration with SDK
```cpp
// main.cpp
HomeAssistantApp* haApp = new HomeAssistantApp();
Core::instance().setAppOrchestrator(haApp);
Core::instance().initialize();
Core::instance().startUITask();

// Main loop
void loop() {
    Core::instance().coordinationLoop();
    vTaskDelay(pdMS_TO_TICKS(50)); // 20Hz
}
```

---

**Made with ❤️ for CloudMouse devices**

*Transform your CloudMouse into your smart house controller!*