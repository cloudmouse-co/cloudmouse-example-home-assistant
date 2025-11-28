#pragma once

#include <lvgl.h>
#include "../hardware/DisplayManager.h"
#include "./services/HomeAssistantService.h"
#include "../hardware/SimpleBuzzer.h"
#include "../core/Events.h"

namespace CloudMouse::Apps
{
    using namespace CloudMouse::App::Services;
    using namespace CloudMouse::Hardware;

    class HomeAssistantApp
    {
    public:
        static void init(lv_group_t *group)
        {
            encoder_group = group;
            createHomeScreen();
        }

        static void show()
        {
            if (screen_home)
            {
                lv_disp_load_scr(screen_home);
                Serial.println("🏠 Home screen loaded");
            }
        }

        static void handleEvent(const CloudMouse::Event &event)
        {
            switch (event.type)
            {
            case EventType::DISPLAY_WAKE_UP:
                show();
                break;

            case EventType::ENCODER_ROTATION:
                // Encoder handled automatically by LVGL
                break;

            case EventType::ENCODER_CLICK:
                // Click handled by button callbacks
                break;

            default:
                break;
            }
        }

    private:
        static lv_group_t *encoder_group;
        static lv_obj_t *screen_home;
        static lv_obj_t *btn_gate;
        static lv_obj_t *btn_shutters;
        static lv_obj_t *btn_lights_off;
        static lv_obj_t *btn_entrance_light;

        static void createHomeScreen()
        {
            screen_home = lv_obj_create(NULL);
            lv_obj_set_style_bg_color(screen_home, lv_color_hex(0x000000), 0);

            // Header
            createHeader(screen_home, "Home Assistant");

            // Grid container
            lv_obj_t *grid = lv_obj_create(screen_home);
            lv_obj_set_size(grid, 460, 240);
            lv_obj_align(grid, LV_ALIGN_CENTER, 0, 20);
            lv_obj_set_style_bg_color(grid, lv_color_hex(0x000000), 0);
            lv_obj_set_style_border_width(grid, 0, 0);
            lv_obj_set_style_pad_all(grid, 10, 0);
            lv_obj_set_layout(grid, LV_LAYOUT_GRID);

            static int32_t col_dsc[] = {220, 220, LV_GRID_TEMPLATE_LAST};
            static int32_t row_dsc[] = {110, 110, LV_GRID_TEMPLATE_LAST};
            lv_obj_set_grid_dsc_array(grid, col_dsc, row_dsc);

            // Button 1: Open Gate (Top Left)
            btn_gate = createButton(grid, 0, 0, 0x2196F3, LV_SYMBOL_HOME "\nOpen Gate", 0);

            // Button 2: Close Shutters (Top Right)
            btn_shutters = createButton(grid, 1, 0, 0x9C27B0, LV_SYMBOL_DOWN "\nClose Shutters", 1);

            // Button 3: All Lights OFF (Bottom Left)
            btn_lights_off = createButton(grid, 0, 1, 0xFF5722, LV_SYMBOL_POWER "\nLights OFF", 2);

            // Button 4: Entrance Light ON (Bottom Right)
            btn_entrance_light = createButton(grid, 1, 1, 0x4CAF50, LV_SYMBOL_CHARGE "\nEntrance ON", 3);

            Serial.println("✅ Home screen created");
        }

        static lv_obj_t *createButton(lv_obj_t *parent, int col, int row, uint32_t color, const char *text, int action)
        {
            lv_obj_t *btn = lv_button_create(parent);
            lv_obj_set_grid_cell(btn, LV_GRID_ALIGN_STRETCH, col, 1, LV_GRID_ALIGN_STRETCH, row, 1);
            lv_obj_set_style_bg_color(btn, lv_color_hex(color), 0);
            lv_obj_set_style_radius(btn, 10, 0);
            lv_obj_add_event_cb(btn, buttonCallback, LV_EVENT_CLICKED, (void *)(intptr_t)action);
            
            // Add to encoder group for navigation
            if (encoder_group)
            {
                lv_group_add_obj(encoder_group, btn);
            }

            lv_obj_t *label = lv_label_create(btn);
            lv_label_set_text(label, text);
            lv_obj_set_style_text_align(label, LV_TEXT_ALIGN_CENTER, 0);
            lv_obj_set_style_text_font(label, &lv_font_montserrat_18, 0);
            lv_obj_center(label);

            return btn;
        }

        static void createHeader(lv_obj_t *parent, const char *title)
        {
            lv_obj_t *header = lv_obj_create(parent);
            lv_obj_set_size(header, 480, 40);
            lv_obj_align(header, LV_ALIGN_TOP_MID, 0, 0);
            lv_obj_set_style_bg_color(header, lv_color_hex(0x222222), 0);
            lv_obj_set_style_border_width(header, 0, 0);
            lv_obj_set_style_radius(header, 0, 0);

            lv_obj_t *label = lv_label_create(header);
            lv_label_set_text(label, title);
            lv_obj_set_style_text_color(label, lv_color_hex(0xFFFFFF), 0);
            lv_obj_center(label);
        }

        static void buttonCallback(lv_event_t *e)
        {
            int action = (int)(intptr_t)lv_event_get_user_data(e);

            Serial.printf("🎮 Button %d pressed!\n", action);

            bool success = false;

            switch (action)
            {
            case 0: // Open Gate
                success = HomeAssistantService::openGate();
                break;
            case 1: // Close Shutters
                success = HomeAssistantService::closeShutters();
                break;
            case 2: // Lights OFF
                success = HomeAssistantService::lightsOff();
                break;
            case 3: // Entrance Light ON
                success = HomeAssistantService::entranceLightOn();
                break;
            }

            if (success)
            {
                SimpleBuzzer::buzz();
            }
            else
            {
                SimpleBuzzer::error();
            }
        }
    };

    // Static members initialization
    lv_group_t *HomeAssistantApp::encoder_group = nullptr;
    lv_obj_t *HomeAssistantApp::screen_home = nullptr;
    lv_obj_t *HomeAssistantApp::btn_gate = nullptr;
    lv_obj_t *HomeAssistantApp::btn_shutters = nullptr;
    lv_obj_t *HomeAssistantApp::btn_lights_off = nullptr;
    lv_obj_t *HomeAssistantApp::btn_entrance_light = nullptr;
}