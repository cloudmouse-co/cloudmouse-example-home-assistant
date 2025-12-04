#pragma once

#include <lvgl.h>
#include "../hardware/DisplayManager.h"
#include "../services/HomeAssistantDataService.h"
#include "../../hardware/SimpleBuzzer.h"
#include "../../core/Events.h"

namespace CloudMouse::App
{
    struct AppEventData;
    class HomeAssistantEntity;
}


namespace CloudMouse::App::Ui
{
    extern "C" {
      LV_FONT_DECLARE(font_awesome_solid_20);
    }

    #define FA_ICON_LIGHT   "\xEF\x83\xAB"  // 0xF0EB - Lightbulb
    #define FA_ICON_CLIMATE "\xEF\x81\xAD"  // 0xF06D - Fire/Thermometer
    #define FA_ICON_SWITCH  "\xEF\x88\x84"  // 0xF204 - Toggle
    #define FA_ICON_COVER   "\xEE\x92\xAF"  // 0xE4AF - Window/Blinds
    #define FA_ICON_SENSOR  "\xEF\x8B\x9B"  // 0xF2DB - Microchip/Sensor

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
        lv_obj_t *screen_loading;
        lv_obj_t *screen_config_needed;
        lv_obj_t *screen_entity_list;
        lv_obj_t *screen_entity_detail;
        lv_obj_t *screen_climate_detail;
        lv_obj_t *screen_switch_detail;

        // template
        lv_obj_t *header_label;
        lv_obj_t *header_list_label;
        lv_obj_t *sidebar_btn_home;
        lv_obj_t *sidebar_btn_light;
        lv_obj_t *sidebar_btn_switch;
        lv_obj_t *sidebar_btn_cover;
        lv_obj_t *sidebar_btn_clima;
        lv_obj_t *sidebar_btn_sensor;
        
        // climate screen items
        lv_obj_t *climate_arc_slider;
        lv_obj_t *climate_label_state;
        lv_obj_t *climate_label_target;
        lv_obj_t *climate_label_target_unit;
        lv_obj_t *climate_label_target_decimal;
        lv_obj_t *climate_label_current;
        lv_obj_t *climate_btn_on;
        lv_obj_t *climate_btn_off;
        bool climate_arc_editing = false;
        int currentTargetValue = 0;

        // switch screen items
        lv_obj_t *switch_btn_on;
        lv_obj_t *switch_btn_off;

        // Entity list items
        lv_obj_t *entity_list_container;
        int selected_entity_index = 0;

        lv_obj_t *btn_gate;
        lv_obj_t *btn_shutters;
        lv_obj_t *btn_lights_off;
        lv_obj_t *btn_entrance_light;

        String currentEntityId;

        static HomeAssistantDisplayManager *instance;
        HomeAssistantPrefs &prefs;

        // Add filter state
        enum class EntityFilter {
            ALL,
            LIGHT,
            SWITCH,
            CLIMA,
            COVER,
            SENSOR,
        };
        
        EntityFilter current_filter = EntityFilter::ALL;
        
        // Filter colors
        struct FilterColors {
            uint32_t bg_color;
            uint32_t border_color;
        };
        
        FilterColors getFilterColors(EntityFilter filter);
        void setActiveFilter(EntityFilter filter);
        void updateSidebarStyles();
        void updateHeaderLabel();

        void onDisplayEvent(const CloudMouse::Event &event);

        void bootstrap();

        void createHomeScreen();
        void createLoadingScreen();
        void createConfigNeededScreen();
        void createEntityListScreen();
        void createEntityDetailScreen();
        void createHeader(lv_obj_t *parent, const char *title);
        void createClimateDetailScreen();
        void createSwitchDetailScreen();
        
        // Navigation
        void showLoading();
        void showConfigNeeded(const String &url);
        void showEntityList();
        void showEntityDetail(int entityIndex);
        void showClimateDetail(const String &entityId);
        void showSwitchDetail(const String &entityId);

        void focusSidebar();
        void focusEntityList();

        // Data hidratation
        void populateEntityList();

        // Helpers
        void updateEntityItem(const String& entityId);
        void updateStateLabel(lv_obj_t* item, std::shared_ptr<HomeAssistantEntity> entityData);
    };

} // namespace CloudMouse::App::Ui