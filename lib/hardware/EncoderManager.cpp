/**
 * CloudMouse SDK - Rotary Encoder Input Manager Implementation
 *
 * Implementation of hardware-accelerated rotary encoder input processing with intelligent
 * button press detection and event-driven architecture for reliable user interaction handling.
 *
 * Implementation Details:
 * - PCNT hardware integration for rotation tracking without CPU interrupts
 * - State machine approach for button press detection with precise timing
 * - Event accumulation strategy for smooth movement reporting across update cycles
 * - Multi-threshold press detection for rich interaction vocabulary
 * - Consumption-based event model preventing duplicate processing
 *
 * Performance Characteristics:
 * - Rotation tracking: Hardware PCNT with 0.25° resolution
 * - Button sampling: 50-100Hz recommended for responsive feel
 * - Memory usage: ~100 bytes RAM for state management
 * - CPU overhead: Minimal due to hardware acceleration
 *
 * Timing Analysis:
 * - Click detection: < 500ms press duration
 * - Long press: 1000-2999ms with optional buzzer feedback
 * - Ultra-long press: >= 3000ms with immediate event trigger
 * - Debouncing: Handled by hardware pull-ups and state machine logic
 */

#include "./EncoderManager.h"
#include "../utils/Logger.h"

namespace CloudMouse::Hardware
{
    // ============================================================================
    // INITIALIZATION AND LIFECYCLE
    // ============================================================================

    EncoderManager::EncoderManager()
        : encoder(ENCODER_CLK_PIN, ENCODER_DT_PIN)
    {
        // Constructor initializes encoder hardware interface
        // Actual GPIO configuration happens in init() method
    }

    void EncoderManager::init()
    {
        SDK_LOGGER("🎮 Initializing EncoderManager...");

        // Configure button pin with internal pull-up resistor
        // Button connects SW pin to ground when pressed (active LOW)
        pinMode(ENCODER_SW_PIN, INPUT_PULLUP);

        // Initialize PCNT-based encoder hardware
        // Sets up quadrature decoder with glitch filtering
        encoder.init();

        // Read initial encoder position and normalize to detent resolution
        // Divide by 4 because PCNT counts 4 edges per physical detent
        lastValue = encoder.position() / 4;

        // Initialize button state by reading current pin level
        // Ensures proper state tracking from startup
        lastButtonState = digitalRead(ENCODER_SW_PIN);

        SDK_LOGGER("✅ EncoderManager initialized successfully\n");
        SDK_LOGGER("🎮 Pin configuration: CLK=%d, DT=%d, SW=%d\n",
                   ENCODER_CLK_PIN, ENCODER_DT_PIN, ENCODER_SW_PIN);
        SDK_LOGGER("🎮 Initial encoder position: %d\n", lastValue);
    }

    // ============================================================================
    // MAIN UPDATE LOOP
    // ============================================================================

    void EncoderManager::update()
    {
        // Process encoder rotation and button state changes
        // Call both processors to ensure comprehensive input handling
        processEncoder();
        processButton();
    }

    // ============================================================================
    // ENCODER ROTATION PROCESSING
    // ============================================================================

    void EncoderManager::processEncoder()
    {
        // Read current encoder position from PCNT hardware
        // Normalize to physical detent resolution (4 counts per detent)
        int newValue = encoder.position() / 4;

        // Check for position change since last update
        if (newValue != lastValue)
        {
            // Calculate movement delta and accumulate for consumption
            // Positive delta = clockwise, negative delta = counter-clockwise
            int delta = newValue - lastValue;
            movement += delta;
            lastValue = newValue;
            movementPending = true;

            // Detect press-and-rotate gesture
            if (isButtonDown() && !pressAndRotateActive)
            {
                SDK_LOGGER("🎮 Press-and-rotate gesture detected!");
                pressAndRotatePending = true;
                pressAndRotateActive = true;

                // Clear all button press tracking - this becomes a different gesture
                clickPending = false;
                longPressPending = false;
                ultraLongPressPending = false;
                waitingForDoubleClick = false;
                longPressBuzzed = false;
                ultraLongPressNotified = false;
            }
        }
    }

    // ============================================================================
    // BUTTON PRESS PROCESSING
    // ============================================================================

    // In EncoderManager.cpp - Update processButton():

    void EncoderManager::processButton()
    {
        bool currentButtonState = digitalRead(ENCODER_SW_PIN);
        unsigned long currentTime = millis();

        // PRESS DETECTION
        if (currentButtonState != lastButtonState && currentButtonState == LOW)
        {
            pressStartTime = currentTime;
            longPressBuzzed = false;
            ultraLongPressNotified = false;
            pressAndRotateActive = false; // Reset press-and-rotate for new press

            SDK_LOGGER("👆 Button press detected");
        }

        // RELEASE DETECTION
        if (currentButtonState != lastButtonState && currentButtonState == HIGH)
        {
            unsigned long pressDuration = currentTime - pressStartTime;
            lastPressDuration = pressDuration;

            SDK_LOGGER("👆 Button released after %lu ms", pressDuration);

            // If press-and-rotate was active, ignore all other button events
            if (pressAndRotateActive)
            {
                SDK_LOGGER("👆 Release ignored (was press-and-rotate gesture)");
                pressAndRotateActive = false;
            }
            else
            {
                // Classify press event based on duration
                if (pressDuration >= ULTRA_LONG_PRESS_DURATION)
                {
                    if (!ultraLongPressNotified)
                    {
                        ultraLongPressPending = true;
                        ultraLongPressNotified = true;
                        SDK_LOGGER("👆🔒🔒 Ultra-long press event (on release)");
                    }
                }
                else if (pressDuration >= LONG_PRESS_DURATION)
                {
                    longPressPending = true;
                    SDK_LOGGER("👆🔒 Long press event");
                }
                else if (pressDuration < CLICK_TIMEOUT)
                {
                    // Double click detection logic
                    if (waitingForDoubleClick)
                    {
                        // Second click arrived in time!
                        doubleClickPending = true;
                        waitingForDoubleClick = false;
                        clickPending = false; // Cancel single click
                        SDK_LOGGER("👆👆 Double click detected!");
                    }
                    else
                    {
                        // First click - start waiting for potential second click
                        waitingForDoubleClick = true;
                        lastClickTime = currentTime;
                        SDK_LOGGER("👆 Click detected, waiting for potential double click...");
                    }
                }
            }
        }

        // ONGOING PRESS FEEDBACK
        if (currentButtonState == LOW && !pressAndRotateActive)
        {
            unsigned long pressTime = getCurrentPressTime();

            if (pressTime >= LONG_PRESS_DURATION && !longPressBuzzed)
            {
                longPressBuzzed = true;
                SDK_LOGGER("🔊 Long press threshold reached");
            }

            if (pressTime >= ULTRA_LONG_PRESS_DURATION && !ultraLongPressNotified)
            {
                ultraLongPressPending = true;
                ultraLongPressNotified = true;
                SDK_LOGGER("👆🔒🔒 Ultra-long press triggered immediately!");
            }
        }
        else
        {
            if (ultraLongPressNotified && lastPressDuration < ULTRA_LONG_PRESS_DURATION)
            {
                ultraLongPressNotified = false;
            }
        }

        // Check double click timeout
        if (waitingForDoubleClick && (currentTime - lastClickTime) > DOUBLE_CLICK_WINDOW)
        {
            // Timeout expired - fire single click
            clickPending = true;
            waitingForDoubleClick = false;
            SDK_LOGGER("👆 Single click confirmed (timeout)");
        }

        lastButtonState = currentButtonState;
    }

    // ============================================================================
    // EVENT CONSUMPTION INTERFACE IMPLEMENTATION
    // ============================================================================

    int EncoderManager::getMovement()
    {
        // Block normal movement if button is pressed or press-and-rotate is active
        if (isButtonDown() || pressAndRotateActive)
        {
            // Don't consume movement - it's for press-and-rotate gesture
            return 0;
        }

        // Return accumulated movement and reset for next consumption cycle
        if (movementPending)
        {
            int result = movement;   // Capture current movement total
            movement = 0;            // Reset accumulator for next cycle
            movementPending = false; // Clear pending flag

            SDK_LOGGER("📊 Movement consumed: %d clicks\n", result);
            return result;
        }

        // No movement pending - return zero
        return 0;
    }

    bool EncoderManager::getClicked()
    {
        // Return click state and reset flag for next consumption cycle
        if (clickPending)
        {
            clickPending = false; // Reset flag after consumption
            SDK_LOGGER("📊 Click event consumed");
            return true;
        }

        // No click pending
        return false;
    }

    bool EncoderManager::getLongPressed()
    {
        // Return long press state and reset flag for next consumption cycle
        if (longPressPending)
        {
            longPressPending = false; // Reset flag after consumption
            SDK_LOGGER("📊 Long press event consumed");
            return true;
        }

        // No long press pending
        return false;
    }

    bool EncoderManager::getUltraLongPressed()
    {
        // Return ultra-long press state and reset flag for next consumption cycle
        if (ultraLongPressPending)
        {
            ultraLongPressPending = false; // Reset flag after consumption
            SDK_LOGGER("📊 Ultra-long press event consumed");
            return true;
        }

        // No ultra-long press pending
        return false;
    }

    bool EncoderManager::getDoubleClicked()
    {
        if (doubleClickPending)
        {
            doubleClickPending = false;
            SDK_LOGGER("📊 Double click event consumed");
            return true;
        }
        return false;
    }

    bool EncoderManager::getPressAndRotate()
    {
        if (pressAndRotatePending)
        {
            pressAndRotatePending = false;
            
            // Reset the movement accumulator since we're consuming it as a gesture
            // The app should call getMovement() separately if it wants the delta
            // but we signal that press-and-rotate happened
            
            SDK_LOGGER("📊 Press-and-rotate event consumed");
            return true;
        }
        return false;
    }

    int EncoderManager::getPressAndRotateMovement()
    {
        // Only return movement if press-and-rotate is active
        if (pressAndRotateActive && movementPending)
        {
            int result = movement;
            movement = 0;
            movementPending = false;
            
            SDK_LOGGER("📊 Press-and-rotate movement: %d clicks", result);
            return result;
        }
        
        return 0;
    }

    // ============================================================================
    // STATE QUERY INTERFACE IMPLEMENTATION (Non-Consuming)
    // ============================================================================

    bool EncoderManager::isButtonDown() const
    {
        // Real-time button state query (LOW = pressed)
        return digitalRead(ENCODER_SW_PIN) == LOW;
    }

    int EncoderManager::getPressTime() const
    {
        // Get current press duration or zero if not pressed
        return getCurrentPressTime();
    }

    int EncoderManager::getLastPressDuration() const
    {
        // Return duration of most recent completed press cycle
        return lastPressDuration;
    }

    bool EncoderManager::resetLastPressDuration()
    {
        if (lastPressDuration > 0)
        {
            lastPressDuration = 0;
            return true;
        }

        return false;
    }

    // ============================================================================
    // INTERNAL HELPER METHODS
    // ============================================================================

    unsigned long EncoderManager::getCurrentPressTime() const
    {
        // Calculate elapsed time since press started, or zero if not pressed
        if (digitalRead(ENCODER_SW_PIN) == LOW)
        {
            // Button currently pressed - calculate elapsed time
            return millis() - pressStartTime;
        }

        // Button not pressed - return zero
        return 0;
    }
}