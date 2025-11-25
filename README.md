# CloudMouse LVGL Example

🎨 **Example implementation of LVGL graphics library on top of CloudMouse SDK**

This project demonstrates how to integrate [LVGL (Light and Versatile Graphics Library)](https://lvgl.io/) with the [CloudMouse SDK](https://github.com/yourusername/cloudmouse-sdk) to create rich, interactive user interfaces on the ESP32-based [CloudMouse](https://cloudmouse.co) device.

## 📋 Overview

This example extends the CloudMouse SDK boilerplate by replacing the basic display rendering with LVGL's powerful widget system. It showcases:

- Multi-screen navigation (Hello World, WiFi Setup, AP Mode, QR Code displays)
- Encoder integration with LVGL input system
- Event-driven architecture between hardware and UI
- Proper LVGL v9 implementation with ESP32 dual-core support

## 🔧 Compatibility

**Arduino IDE** - out-of-the-box.
**Platformio** - with source code switching (see below!).

### Important: Source Code Switching

> 💡 The project maintains a single codebase that works with both Arduino IDE and PlatformIO. The `src/main.cpp` file is kept in sync but needs to be toggled:

**To use PlatformIO:**
1. Open `src/main.cpp` in your editor
2. **Uncomment the entire file**
3. Save and build with PlatformIO

**To switch back to Arduino IDE:**
1. Open `src/main.cpp` in your editor
2. **Re-comment the entire file**
3. Save and build with Arduino IDE

> 💡 **Pro tip**: Most editors support block comment toggling with `Ctrl+/` (Windows/Linux) or `Cmd+/` (Mac). Select all (`Ctrl+A`) then toggle comments!


## 📚 Prerequisites

Before using this example, make sure you have:

1. **CloudMouse SDK** - Base SDK with hardware abstraction layer
   - 📖 [CloudMouse SDK Repository](https://github.com/tibonilab/cloudmouse-boilerplate)
   
2. **LVGL Library** - Graphics library (v9.x recommended)
   - 📖 [LVGL Official Documentation](https://docs.lvgl.io/)
   - Install via Arduino Library Manager: `LVGL`

3. **Required Hardware**
   - CloudMouse device

## 🚀 Setup Instructions

### 1. Install Dependencies

Install the following libraries via Arduino Library Manager:
- `LVGL` (v9.x)

### 2. Configure LVGL

**IMPORTANT:** You need to add the `lv_conf.h` configuration file to your Arduino libraries folder, as [mentioned here](https://docs.lvgl.io/8/get-started/platforms/arduino.html#configure-lvgl).

**Location:**
```
~/Arduino/libraries/lv_conf.h
```

> ℹ️ A reference configuration is available in this repository at `lib/config/lv_conf.h` for guidance.

Or you can copy and paste the following config snippet.

```cpp
#ifndef LV_CONF_H
#define LV_CONF_H

#define LV_USE_PERF_MONITOR 1
#define LV_USE_MEM_MONITOR 1

#define LV_COLOR_DEPTH 16
#define LV_COLOR_16_SWAP 1
#define LV_MEM_SIZE (48U * 1024U)

#define LV_FONT_MONTSERRAT_12 1
#define LV_FONT_MONTSERRAT_14 1
#define LV_FONT_MONTSERRAT_16 1
#define LV_FONT_MONTSERRAT_18 1
#define LV_FONT_MONTSERRAT_20 1
#define LV_FONT_MONTSERRAT_24 1
#define LV_FONT_MONTSERRAT_28 1
#define LV_FONT_MONTSERRAT_32 1
#define LV_FONT_MONTSERRAT_36 1
#define LV_FONT_MONTSERRAT_48 1

#define LV_FONT_DEFAULT &lv_font_montserrat_14
#define LV_TXT_ENC LV_TXT_ENC_UTF8

#define LV_TICK_CUSTOM 1
#define LV_TICK_CUSTOM_INCLUDE "Arduino.h"
#define LV_TICK_CUSTOM_SYS_TIME_EXPR (millis())

#define LV_USE_LOG 1
#define LV_LOG_LEVEL LV_LOG_LEVEL_WARN
#define LV_LOG_PRINTF 1

#define LV_USE_ASSERT_NULL 1
#define LV_USE_ASSERT_MALLOC 1
#define LV_USE_ASSERT_STYLE 0
#define LV_USE_ASSERT_MEM_INTEGRITY 0
#define LV_USE_ASSERT_OBJ 0

#define LV_USE_LABEL 1
#define LV_USE_BUTTON 1
#define LV_USE_IMAGE 1
#define LV_USE_ARC 1
#define LV_USE_BAR 1
#define LV_USE_FLEX 1
#define LV_USE_GRID 1
#define LV_USE_PSRAM 1

#define LV_USE_SPINNER 1
#define LV_USE_QRCODE 1
#define LV_USE_LABEL 1

#endif
```


### 3. Clone and Open

```bash
git clone https://github.com/cloudmouse-co/cloudmouse-example-lvgl
```

Open the `.ino` file in Arduino IDE.

### 4. Upload

Select your ESP32 board and upload the sketch.

## 🏗️ Architecture

This example follows the [CloudMouse SDK](https://github.com/cloudmouse-co/cloudmouse-sdk) architecture:

```
┌─────────────────┐
│   Arduino IDE   │
│    (.ino)       │
└────────┬────────┘
         │
    ┌────▼─────┐
    │   Core   │ ◄─── Event Bus
    └────┬─────┘
         │
    ┌────▼──────────────────┐
    │  DisplayManager       │
    │  (LVGL Integration)   │
    └───────────────────────┘
```

**Key Components:**

- **DisplayManager.cpp** - Main LVGL integration with CloudMouse hardware
- **LVGL Ticker** - Global timer for LVGL tick updates (5ms interval)
- **Event System** - Bridges hardware events (encoder, WiFi) to UI

## 📖 Key Files

| File | Description |
|------|-------------|
| [lib/hardware/DisplayManager.cpp](https://github.com/tibonilab/cloudmouse-example-lvgl/blob/main/lib/hardware/DisplayManager.cpp) | LVGL implementation and screen management |
| [lib/hardware/DisplayManager.h](https://github.com/tibonilab/cloudmouse-example-lvgl/blob/main/lib/hardware/DisplayManager.h) | DisplayManager interface with LVGL ticker setup |

## 🎯 Features Demonstrated

### Screens
- **Hello World** - Welcome screen with encoder interaction
- **WiFi Connecting** - Connection status with spinner animation
- **AP Mode** - Access Point setup with QR code
- **AP Connected** - Configuration portal access with QR code

### LVGL Integration
- Custom display driver for ILI9488 via LovyanGFX
- Encoder input device for navigation
- PSRAM buffer allocation for optimal performance
- Proper v9 API usage with dual buffers

### Event Handling
- Encoder rotation events
- Encoder click/long-press events
- WiFi state transitions
- Screen navigation logic

## 🔗 Useful Links

- [LVGL Official Website](https://lvgl.io/)
- [LVGL Documentation](https://docs.lvgl.io/)
- [LVGL Examples](https://docs.lvgl.io/master/examples.html)
- [CloudMouse SDK](https://github.com/cloudmouse-co/cloudmouse-sdk)


## 📝 License

This example follows the same license as the CloudMouse SDK.

## 🤝 Contributing

Contributions are welcome! Feel free to open issues or submit pull requests to improve this example.

---

Made with ❤️ for the CloudMouse community
