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
    extern "C"
    {
        LV_FONT_DECLARE(font_awesome_solid_20);
        LV_FONT_DECLARE(font_awesome_solid_48);
    }

#define FA_ICON_LIGHT "\xEF\x83\xAB"   // 0xF0EB - Lightbulb
#define FA_ICON_CLIMATE "\xEF\x81\xAD" // 0xF06D - Fire/Thermometer
#define FA_ICON_SWITCH "\xEF\x88\x84"  // 0xF204 - Toggle
#define FA_ICON_COVER "\xEE\x92\xAF"   // 0xE4AF - Window/Blinds
#define FA_ICON_SENSOR "\xEF\x8B\x9B"  // 0xF2DB - Microchip/Sensor

#define FA_SUN "\xEF\x86\x85"
#define FA_SUN_CLOUD "\xEF\x9B\x84"
#define FA_CLOUD "\xEF\x83\x82"
#define FA_CLOUD_RAIN "\xEF\x9D\x80"
#define FA_SNOWFLAKE "\xEF\x8B\x9C"
#define FA_BOLT_CLOUD "\xEF\x9D\xAC"
#define FA_SMOG "\xEF\x9D\xA9"

    using namespace CloudMouse::App::Services;
    using namespace CloudMouse::App;

    enum class ViewType
    {
        LOADING,
        CONFIG_NEEDED,
        ENTITY_LIST,
        CLIMATE_DETAIL,
        SWITCH_DETAIL,
        LIGHT_DETAIL,
        SENSOR_DETAIL,
        COVER_DETAIL,
        DASHBOARD,
    };

    class HomeAssistantDisplayManager
    {
    public:
        HomeAssistantDisplayManager(HomeAssistantPrefs &preferences);
        ~HomeAssistantDisplayManager();

        void init();

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

        void startTimeUpdates();
        void stopTimeUpdates();

    private:
        ViewType current_view = ViewType::LOADING;
        lv_timer_t *time_update_timer;

        void renderEntityList();
        void renderEntityDetail(const String &entityId);
        void renderSwitchDetail(const String &entityId);
        void renderLightDetail(const String &entityId);
        void renderSensorDetail(const String &entityId);
        void renderClimateDetail(const String &entityId);
        void renderCoverDetail(const String &entityId);
        void renderLoading();
        void resetContentContainer();
        void renderDashboard();

        static void timeUpdateCallback(lv_timer_t *timer);
        void updateTime();

        lv_group_t *encoder_group;

        // template
        lv_obj_t *screen_main;
        lv_obj_t *content_container;

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
        lv_obj_t *switch_status_icon;

        // light screen items
        lv_obj_t *light_btn_on;
        lv_obj_t *light_btn_off;
        lv_obj_t *light_status_icon;

        // sensor screen items
        lv_obj_t *sensor_status_icon;

        // cover screen items
        lv_obj_t *cover_btn_up;
        lv_obj_t *cover_btn_off;
        lv_obj_t *cover_btn_dwn;

        // dashboard
        lv_obj_t *dateLabelDay;
        lv_obj_t *dateLabelDate;
        lv_obj_t *timeLabel;
        lv_obj_t *labelForecastIcon;
        lv_obj_t *labelForecastWeather;
        lv_obj_t *labelForecastTemperature;

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
        enum class EntityFilter
        {
            ALL,
            LIGHT,
            SWITCH,
            CLIMA,
            COVER,
            SENSOR,
        };

        EntityFilter current_filter = EntityFilter::ALL;

        // Filter colors
        struct FilterColors
        {
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
        void createMainScreen();
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
        void showEntityDetail(const String &entityId);
        void showClimateDetail(const String &entityId);
        void showSwitchDetail(const String &entityId);

        void focusSidebar();
        void focusEntityList();

        // Data hidratation
        void populateEntityList();

        // Helpers
        void updateEntityItem(const String &entityId);
        void updateStateLabel(lv_obj_t *item, std::shared_ptr<HomeAssistantEntity> entityData);
        const char* getWeatherIconFA(const char* state);
    };

} // namespace CloudMouse::App::Ui