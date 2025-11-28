#pragma once

#include <lvgl.h>
#include "../hardware/DisplayManager.h"
#include "../services/HomeAssistantDataService.h"
#include "../../hardware/SimpleBuzzer.h"
#include "../../core/Events.h"

namespace CloudMouse::App
{
    struct AppEventData;
}

namespace CloudMouse::App::Ui
{
    using namespace CloudMouse::App::Services;
    using namespace CloudMouse::App;

    class HomeAssistantDisplayManager
    {
    public:
        HomeAssistantDisplayManager(HomeAssistantPrefs &preferences);
        ~HomeAssistantDisplayManager();

        void init();
        // void show();

        void processAppEvent(const AppEventData &event);

        // Static callback wrapper for SDK DisplayManager
        // This gets called from Core 1 when DisplayManager processes events
        static void handleDisplayCallback(const CloudMouse::Event &event)
        {
            if (!instance)
            {
                return;
            }

            instance->onDisplayEvent(event);
        }

    private:
        lv_group_t *encoder_group;

        // screens
        lv_obj_t *screen_home;
        lv_obj_t *screen_config_needed;
        lv_obj_t *screen_entity_list;
        lv_obj_t *screen_entity_detail;
        lv_obj_t *screen_climate_detail;

        // climate screen items
        lv_obj_t *climate_arc_slider;
        lv_obj_t *climate_label_state;
        lv_obj_t *climate_label_target;
        lv_obj_t *climate_label_current;
        lv_obj_t *climate_btn_on;
        lv_obj_t *climate_btn_off;
        bool climate_arc_editing = false;

        // Entity list items
        lv_obj_t *entity_list_container;
        int selected_entity_index = 0;

        lv_obj_t *btn_gate;
        lv_obj_t *btn_shutters;
        lv_obj_t *btn_lights_off;
        lv_obj_t *btn_entrance_light;

        static HomeAssistantDisplayManager *instance;
        HomeAssistantPrefs &prefs;

        void onDisplayEvent(const CloudMouse::Event &event);

        void bootstrap();

        void createHomeScreen();
        void createConfigNeededScreen();
        void createEntityListScreen();
        void createEntityDetailScreen();
        void createHeader(lv_obj_t *parent, const char *title);
        void createClimateDetailScreen();
        
        // Navigation
        void showConfigNeeded(const String &url);
        void showEntityList();
        void showEntityDetail(int entityIndex);
        void showClimateDetail(const String &entityId);
        
        void populateEntityList();
    };

} // namespace CloudMouse::App::Ui