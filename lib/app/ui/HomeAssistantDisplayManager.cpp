#include "HomeAssistantDisplayManager.h"
#include "../HomeAssistantApp.h"
#include "../model/HomeAssistantAppStore.h"
#include "../model/HomeAssistantEntity.h"

namespace CloudMouse::App::Ui
{

    using namespace CloudMouse::App;

    // Initialize static instance pointer
    HomeAssistantDisplayManager *HomeAssistantDisplayManager::instance = nullptr;

    HomeAssistantDisplayManager::HomeAssistantDisplayManager(HomeAssistantPrefs &preferences) : prefs(preferences)
    {
        instance = this;
    }
    HomeAssistantDisplayManager::~HomeAssistantDisplayManager()
    {
        instance = nullptr;
    }

    void HomeAssistantDisplayManager::init()
    {
        APP_LOGGER("Initializing Display Manager...");

        // Register callback with SDK DisplayManager
        // This allows us to receive ALL events that DisplayManager processes
        CloudMouse::Core::instance().getDisplay()->registerAppCallback(
            &HomeAssistantDisplayManager::handleDisplayCallback);

        APP_LOGGER("Display manager initialized gracefully!");
    }

    void HomeAssistantDisplayManager::bootstrap()
    {
        APP_LOGGER("Display manager BOOTSTRAP");

        encoder_group = lv_group_get_default();
        if (!encoder_group)
        {
            APP_LOGGER("⚠️ No default encoder group, creating one");
            encoder_group = lv_group_create();
            lv_group_set_default(encoder_group);
        }

        createMainScreen();
        createConfigNeededScreen();

        APP_LOGGER("Display manager BOOTSTRAP completed");
    }

    void HomeAssistantDisplayManager::processAppEvent(const AppEventData &event)
    {
        switch (event.type)
        {
        case AppEventType::DISPLAY_BOOTSTRAP:
            APP_LOGGER("RECEIVED DISPLAY_BOOTSTRAP");
            bootstrap();
            break;

        case AppEventType::SHOW_CONFIG_NEEDED:
            APP_LOGGER("RECEIVED SHOW_CONFIG_NEEDED");
            showConfigNeeded(event.stringData);
            break;

        case AppEventType::SHOW_ENTITY_LIST:
            APP_LOGGER("RECEIVED SHOW_ENTITY_LIST");
            showEntityList();
            break;

        case AppEventType::CONFIG_SET:
            APP_LOGGER("RECEIVED CONFIG_SET - populating entity list");
            showEntityList();
            break;

        case AppEventType::ENTITY_UPDATED:
            APP_LOGGER("RECEIVED ENTITY_UPDATED for: %s", event.getStringData().c_str());
            updateEntityItem(event.getStringData());
            break;

        case AppEventType::SHOW_LOADING:
            APP_LOGGER("RECEIVED SHOW_LOADING");
            showLoading();
            break;

        case AppEventType::DISPLAY_UPLEVEL:
            APP_LOGGER("RECEIVED DISPLAY_UPLEVEL");
            if (current_view == ViewType::ENTITY_LIST)
            {
                focusSidebar();
            }
            else
            {
                showEntityList();
            }
            break;

        default:
            break;
        }
    }

    void HomeAssistantDisplayManager::onDisplayEvent(const CloudMouse::Event &event)
    {
        if (isAppEvent(event))
        {
            processAppEvent(toAppEvent(event));
        }

        switch (event.type)
        {

        case CloudMouse::EventType::ENCODER_DOUBLE_CLICK:
        {
            APP_LOGGER("ENCODER DOUBLE CLICK");

            if (current_view == ViewType::ENTITY_LIST)
            {
                lv_obj_t *focused = lv_group_get_focused(encoder_group);

                if (focused)
                {
                    // Get entity_id directly from user_data (already stored correctly!)
                    const char *entityId = (const char *)lv_obj_get_user_data(focused);

                    if (entityId)
                    {
                        APP_LOGGER("Selected entity: %s", entityId);

                        CloudMouse::EventBus::instance().sendToMain(
                            toSDKEvent(AppEventData::fetchEntityStatus(entityId)));

                        currentEntityId = String(entityId);

                        showEntityDetail(entityId);
                    }
                }
            }

            break;

        }
        

        case CloudMouse::EventType::ENCODER_CLICK:
        {
            APP_LOGGER("ENCODER CLICK");

            if (current_view == ViewType::ENTITY_LIST)
            {
                lv_obj_t *focused = lv_group_get_focused(encoder_group);

                if (focused)
                {
                    const char * entityId = (const char *)lv_obj_get_user_data(focused);

                    // Check for "toggle action" if exists call it...
                    if (String(entityId).startsWith("light."))
                    {
                        auto entity = AppStore::instance().getEntity(entityId);

                        if (String(entity->getState()).equals("on"))
                        {
                            CloudMouse::EventBus::instance().sendToMain(toSDKEvent(AppEventData::callLightOff(entityId)));
                        }
                        else
                        {
                            CloudMouse::EventBus::instance().sendToMain(toSDKEvent(AppEventData::callLightOn(entityId)));
                        }
                    }
                    else if (String(entityId).startsWith("switch."))
                    {
                        auto entity = AppStore::instance().getEntity(entityId);

                        if (String(entity->getState()).equals("on"))
                        {
                            CloudMouse::EventBus::instance().sendToMain(toSDKEvent(AppEventData::callSwitchOff(entityId)));
                        }
                        else
                        {
                            CloudMouse::EventBus::instance().sendToMain(toSDKEvent(AppEventData::callSwitchOn(entityId)));
                        }
                    }
                    //.. otherwise show detail
                    else 
                    {
                        // Get entity_id directly from user_data (already stored correctly!)
    
                        if (entityId)
                        {
                            APP_LOGGER("Selected entity: %s", entityId);
    
                            CloudMouse::EventBus::instance().sendToMain(
                                toSDKEvent(AppEventData::fetchEntityStatus(entityId)));
    
                            currentEntityId = String(entityId);
    
                            showEntityDetail(entityId);
                        }
                    }
                }
            }
            else if (current_view == ViewType::CLIMATE_DETAIL)
            {
                lv_obj_t *focused = lv_group_get_focused(encoder_group);

                if (focused == climate_arc_slider)
                {
                    // Toggle editing mode
                    climate_arc_editing = !climate_arc_editing;

                    if (climate_arc_editing)
                    {
                        lv_obj_add_state(climate_arc_slider, LV_STATE_EDITED);
                        APP_LOGGER("Arc editing: ON");
                    }
                    else
                    {
                        lv_obj_remove_state(climate_arc_slider, LV_STATE_EDITED);
                        APP_LOGGER("Arc editing: OFF");
                    }
                }
                else if (focused == climate_btn_on)
                {
                    APP_LOGGER("ON button clicked!");
                    // TODO: call HA service
                }
                else if (focused == climate_btn_off)
                {
                    APP_LOGGER("OFF button clicked!");
                    // TODO: call HA service
                }
            }
            else if (current_view == ViewType::SWITCH_DETAIL)
            {
                lv_obj_t *focused = lv_group_get_focused(encoder_group);
                if (focused == switch_btn_on)
                {
                    APP_LOGGER("ON button clicked!");
                    CloudMouse::EventBus::instance().sendToMain(toSDKEvent(AppEventData::callSwitchOn(currentEntityId)));
                }
                else if (focused == switch_btn_off)
                {
                    APP_LOGGER("OFF button clicked!");
                    CloudMouse::EventBus::instance().sendToMain(toSDKEvent(AppEventData::callSwitchOff(currentEntityId)));
                }
            }
            else if (current_view == ViewType::LIGHT_DETAIL)
            {
                lv_obj_t *focused = lv_group_get_focused(encoder_group);
                if (focused == light_btn_on)
                {
                    APP_LOGGER("ON button clicked!");
                    CloudMouse::EventBus::instance().sendToMain(toSDKEvent(AppEventData::callLightOn(currentEntityId)));
                }
                else if (focused == light_btn_off)
                {
                    APP_LOGGER("OFF button clicked!");
                    CloudMouse::EventBus::instance().sendToMain(toSDKEvent(AppEventData::callLightOff(currentEntityId)));
                }
            }
            else if (current_view == ViewType::COVER_DETAIL)
            {
                lv_obj_t *focused = lv_group_get_focused(encoder_group);
                if (focused == cover_btn_up)
                {
                    APP_LOGGER("OPEN button clicked!");
                    CloudMouse::EventBus::instance().sendToMain(toSDKEvent(AppEventData::callCoverOpen(currentEntityId)));
                }
                else if (focused == cover_btn_dwn)
                {
                    APP_LOGGER("CLOSE button clicked!");
                    CloudMouse::EventBus::instance().sendToMain(toSDKEvent(AppEventData::callCoverClose(currentEntityId)));
                }
                else 
                {
                    APP_LOGGER("STOP button clicked!");
                    CloudMouse::EventBus::instance().sendToMain(toSDKEvent(AppEventData::callCoverStop(currentEntityId)));
                }
            }
            else if (current_view == ViewType::SENSOR_DETAIL)
            {
                showEntityList();
            }

            break;
        }

        case CloudMouse::EventType::ENCODER_ROTATION:
        {
            APP_LOGGER("ENCODER ROTATION: %d", event.value);

            if (current_view == ViewType::CLIMATE_DETAIL)
            {
                lv_obj_t *focused = lv_group_get_focused(encoder_group);
                if (!focused)
                {
                    lv_group_focus_obj(climate_arc_slider);
                }

                if (climate_arc_editing)
                {
                    int newValue = currentTargetValue + event.value;

                    // Clamp tra 150-300
                    if (newValue < 150)
                        newValue = 150;
                    if (newValue > 300)
                        newValue = 300;

                    lv_arc_set_value(climate_arc_slider, newValue);
                    currentTargetValue = newValue;

                    int parte_intera = newValue / 10;
                    int parte_decimale = newValue % 10;

                    lv_label_set_text_fmt(climate_label_target, "%d", parte_intera);
                    lv_label_set_text_fmt(climate_label_target_decimal, ",%d", parte_decimale);
                }
            }
            break;
        }

        case CloudMouse::EventType::ENCODER_LONG_PRESS:
        {
            APP_LOGGER("ENCODER LONG PRESS");
            break;
        }

        default:
            break;
        }
    }

    void HomeAssistantDisplayManager::createHeader(lv_obj_t *parent, const char *title)
    {
        lv_obj_t *header = lv_obj_create(parent);
        lv_obj_set_size(header, 480, 40);
        lv_obj_align(header, LV_ALIGN_TOP_MID, 0, 0);
        lv_obj_set_style_bg_color(header, lv_color_hex(0x000000), 0);
        lv_obj_set_style_border_width(header, 0, 0);
        lv_obj_set_style_radius(header, 0, 0);
        lv_obj_set_scrollbar_mode(header, LV_SCROLLBAR_MODE_OFF);

        header_label = lv_label_create(header);
        lv_label_set_text(header_label, title);
        lv_obj_set_style_text_color(header_label, lv_color_hex(0xFFFFFF), 0);
        lv_obj_center(header_label);
    }

    // ============================================================================
    // SCREEN 1: CONFIG NEEDED (con QR code)
    // ============================================================================

    void HomeAssistantDisplayManager::createConfigNeededScreen()
    {
        screen_config_needed = lv_obj_create(NULL);
        lv_obj_set_style_bg_color(screen_config_needed, lv_color_hex(0x000000), 0);

        createHeader(screen_config_needed, "");

        // Container principale
        lv_obj_t *container = lv_obj_create(screen_config_needed);
        lv_obj_set_size(container, 460, 300);
        lv_obj_align(container, LV_ALIGN_CENTER, 0, 0);
        lv_obj_set_style_bg_color(container, lv_color_hex(0x111111), 0);
        lv_obj_set_style_border_width(container, 2, 0);
        lv_obj_set_style_border_color(container, lv_color_hex(0xFF9800), 0);
        lv_obj_set_style_pad_all(container, 25, 0);
        lv_obj_set_flex_flow(container, LV_FLEX_FLOW_COLUMN);
        lv_obj_set_flex_align(container, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
        lv_obj_set_scrollbar_mode(container, LV_SCROLLBAR_MODE_OFF);

        // Icona warning
        lv_obj_t *icon = lv_label_create(container);
        lv_label_set_text(icon, LV_SYMBOL_WARNING);
        lv_obj_set_style_text_font(icon, &lv_font_montserrat_48, 0);
        lv_obj_set_style_text_color(icon, lv_color_hex(0xFF9800), 0);

        // Messaggio
        lv_obj_t *msg = lv_label_create(container);
        lv_label_set_text(msg, "Please configure your device");
        lv_obj_set_style_text_align(msg, LV_TEXT_ALIGN_CENTER, 0);
        lv_obj_set_style_text_font(msg, &lv_font_montserrat_18, 0);
        lv_obj_set_style_text_color(msg, lv_color_hex(0xFFFFFF), 0);

        // URL label (verrà aggiornato dinamicamente)
        lv_obj_t *url_label = lv_label_create(container);
        lv_obj_set_user_data(container, url_label); // salva riferimento per update
        lv_label_set_text(url_label, "http://...");
        lv_obj_set_style_text_font(url_label, &lv_font_montserrat_14, 0);
        lv_obj_set_style_text_color(url_label, lv_color_hex(0x2196F3), 0);

        // QR Code placeholder (lo creiamo vuoto, lo popoleremo dopo)
        lv_obj_t *qr = lv_qrcode_create(container);
        lv_qrcode_set_size(qr, 120);
        lv_qrcode_set_dark_color(qr, lv_color_hex(0xFFFFFF));
        lv_qrcode_set_light_color(qr, lv_color_hex(0x000000));
        lv_obj_set_user_data(url_label, qr); // salva riferimento QR per update

        APP_LOGGER("✅ Config needed screen created");
    }

    void HomeAssistantDisplayManager::showConfigNeeded(const String &url)
    {
        // Aggiorna URL
        lv_obj_t *url_label = (lv_obj_t *)lv_obj_get_user_data(
            lv_obj_get_child(screen_config_needed, 1)); // container

        lv_label_set_text(url_label, url.c_str());

        // Aggiorna QR code
        lv_obj_t *qr = (lv_obj_t *)lv_obj_get_user_data(url_label);
        lv_qrcode_update(qr, url.c_str(), url.length());

        lv_screen_load(screen_config_needed);
    }

    // ============================================================================
    // SCREEN 2: ENTITY LIST
    // ============================================================================

    void HomeAssistantDisplayManager::createMainScreen()
    {
        screen_main = lv_obj_create(NULL);
        lv_obj_set_style_bg_color(screen_main, lv_color_hex(0x000000), 0);

        // createHeader(screen_entity_list, "Entities");

        // SIDEBAR
        lv_obj_t *sidebar_container = lv_obj_create(screen_main);
        lv_obj_set_size(sidebar_container, 70, 320);
        lv_obj_align(sidebar_container, LV_ALIGN_TOP_LEFT, 0, 0);
        lv_obj_set_style_bg_color(sidebar_container, lv_color_hex(0x000000), 0);
        lv_obj_set_style_border_width(sidebar_container, 0, 0);
        lv_obj_set_style_pad_all(sidebar_container, 5, 0);
        lv_obj_set_flex_flow(sidebar_container, LV_FLEX_FLOW_COLUMN);
        lv_obj_set_scrollbar_mode(sidebar_container, LV_SCROLLBAR_MODE_OFF);

        sidebar_btn_home = lv_button_create(sidebar_container);
        lv_obj_set_size(sidebar_btn_home, 54, 54);
        lv_obj_align(sidebar_btn_home, LV_ALIGN_CENTER, 5, 0);
        lv_obj_set_style_bg_color(sidebar_btn_home, lv_color_hex(0x0d6079), 0);
        lv_obj_set_style_shadow_width(sidebar_btn_home, 0, LV_PART_MAIN);
        lv_obj_set_style_radius(sidebar_btn_home, LV_RADIUS_CIRCLE, LV_PART_MAIN);
        lv_obj_add_flag(sidebar_btn_home, LV_OBJ_FLAG_CLICKABLE);

        // lv_obj_set_style_bg_color(sidebar_btn_home, lv_color_hex(0x535353), LV_STATE_FOCUSED);

        lv_obj_add_event_cb(sidebar_btn_home, [](lv_event_t *e)
                            {
        auto *manager = (HomeAssistantDisplayManager*)lv_event_get_user_data(e);
        manager->setActiveFilter(EntityFilter::ALL); }, LV_EVENT_CLICKED, this);

        lv_obj_t *label_home = lv_label_create(sidebar_btn_home);
        lv_label_set_text(label_home, LV_SYMBOL_HOME);
        lv_obj_set_style_text_color(label_home, lv_color_hex(0xFFFFFF), 0);
        lv_obj_set_style_text_font(label_home, &lv_font_montserrat_24, 0);
        lv_obj_center(label_home);

        sidebar_btn_light = lv_button_create(sidebar_container);
        lv_obj_set_size(sidebar_btn_light, 54, 54);
        lv_obj_align(sidebar_btn_light, LV_ALIGN_CENTER, 64, 0);
        // lv_obj_set_style_bg_color(sidebar_btn_light, lv_color_hex(0x8936ec), 0);
        lv_obj_set_style_bg_color(sidebar_btn_light, lv_color_hex(0x323232), 0);
        lv_obj_set_style_shadow_width(sidebar_btn_light, 0, LV_PART_MAIN);
        lv_obj_set_style_radius(sidebar_btn_light, LV_RADIUS_CIRCLE, LV_PART_MAIN);
        lv_obj_add_flag(sidebar_btn_light, LV_OBJ_FLAG_CLICKABLE);
        // lv_obj_set_style_bg_color(sidebar_btn_light, lv_color_hex(0x535353), LV_STATE_FOCUSED);

        lv_obj_add_event_cb(sidebar_btn_light, [](lv_event_t *e)
                            {
        auto *manager = (HomeAssistantDisplayManager*)lv_event_get_user_data(e);
        manager->setActiveFilter(EntityFilter::LIGHT); }, LV_EVENT_CLICKED, this);

        lv_obj_t *label_light = lv_label_create(sidebar_btn_light);
        lv_label_set_text(label_light, FA_ICON_LIGHT);
        lv_obj_set_style_text_font(label_light, &font_awesome_solid_20, 0);
        lv_obj_set_style_text_color(label_light, lv_color_hex(0xFFFFFF), 0);
        lv_obj_center(label_light);

        sidebar_btn_switch = lv_button_create(sidebar_container);
        lv_obj_set_size(sidebar_btn_switch, 54, 54);
        lv_obj_align(sidebar_btn_switch, LV_ALIGN_CENTER, 128, 0);
        // lv_obj_set_style_bg_color(sidebar_btn_switch, lv_color_hex(0x4b57f8), 0);
        lv_obj_set_style_bg_color(sidebar_btn_switch, lv_color_hex(0x323232), 0);
        lv_obj_set_style_shadow_width(sidebar_btn_switch, 0, LV_PART_MAIN);
        lv_obj_set_style_radius(sidebar_btn_switch, LV_RADIUS_CIRCLE, LV_PART_MAIN);
        lv_obj_add_flag(sidebar_btn_switch, LV_OBJ_FLAG_CLICKABLE);
        // lv_obj_set_style_bg_color(sidebar_btn_switch, lv_color_hex(0x535353), LV_STATE_FOCUSED);

        lv_obj_add_event_cb(sidebar_btn_switch, [](lv_event_t *e)
                            {
        auto *manager = (HomeAssistantDisplayManager*)lv_event_get_user_data(e);
        manager->setActiveFilter(EntityFilter::SWITCH); }, LV_EVENT_CLICKED, this);

        lv_obj_t *label_switch = lv_label_create(sidebar_btn_switch);
        lv_label_set_text(label_switch, FA_ICON_SWITCH);
        lv_obj_set_style_text_color(label_switch, lv_color_hex(0xFFFFFF), 0);
        lv_obj_set_style_text_font(label_switch, &font_awesome_solid_20, 0);
        lv_obj_center(label_switch);

        sidebar_btn_clima = lv_button_create(sidebar_container);
        lv_obj_set_size(sidebar_btn_clima, 54, 54);
        lv_obj_align(sidebar_btn_clima, LV_ALIGN_CENTER, 0, 0);
        lv_obj_set_style_bg_color(sidebar_btn_clima, lv_color_hex(0x323232), 0);
        lv_obj_set_style_shadow_width(sidebar_btn_clima, 0, LV_PART_MAIN);
        lv_obj_set_style_radius(sidebar_btn_clima, LV_RADIUS_CIRCLE, LV_PART_MAIN);
        lv_obj_add_flag(sidebar_btn_clima, LV_OBJ_FLAG_CLICKABLE);

        // lv_obj_set_style_bg_color(sidebar_btn_home, lv_color_hex(0x535353), LV_STATE_FOCUSED);

        lv_obj_add_event_cb(sidebar_btn_clima, [](lv_event_t *e)
                            {
        auto *manager = (HomeAssistantDisplayManager*)lv_event_get_user_data(e);
        manager->setActiveFilter(EntityFilter::CLIMA); }, LV_EVENT_CLICKED, this);

        lv_obj_t *label_clima = lv_label_create(sidebar_btn_clima);
        lv_label_set_text(label_clima, FA_ICON_CLIMATE);
        lv_obj_set_style_text_color(label_clima, lv_color_hex(0xFFFFFF), 0);
        lv_obj_set_style_text_font(label_clima, &font_awesome_solid_20, 0);
        lv_obj_center(label_clima);

        sidebar_btn_cover = lv_button_create(sidebar_container);
        lv_obj_set_size(sidebar_btn_cover, 54, 54);
        lv_obj_align(sidebar_btn_cover, LV_ALIGN_CENTER, 0, 0);
        lv_obj_set_style_bg_color(sidebar_btn_cover, lv_color_hex(0x323232), 0);
        lv_obj_set_style_shadow_width(sidebar_btn_cover, 0, LV_PART_MAIN);
        lv_obj_set_style_radius(sidebar_btn_cover, LV_RADIUS_CIRCLE, LV_PART_MAIN);
        lv_obj_add_flag(sidebar_btn_cover, LV_OBJ_FLAG_CLICKABLE);

        // lv_obj_set_style_bg_color(sidebar_btn_home, lv_color_hex(0x535353), LV_STATE_FOCUSED);

        lv_obj_add_event_cb(sidebar_btn_cover, [](lv_event_t *e)
                            {
        auto *manager = (HomeAssistantDisplayManager*)lv_event_get_user_data(e);
        manager->setActiveFilter(EntityFilter::COVER); }, LV_EVENT_CLICKED, this);

        lv_obj_t *label_cover = lv_label_create(sidebar_btn_cover);
        lv_label_set_text(label_cover, FA_ICON_COVER);
        lv_obj_set_style_text_color(label_cover, lv_color_hex(0xFFFFFF), 0);
        lv_obj_set_style_text_font(label_cover, &font_awesome_solid_20, 0);
        lv_obj_center(label_cover);

        sidebar_btn_sensor = lv_button_create(sidebar_container);
        lv_obj_set_size(sidebar_btn_sensor, 54, 54);
        lv_obj_align(sidebar_btn_sensor, LV_ALIGN_CENTER, 0, 0);
        lv_obj_set_style_bg_color(sidebar_btn_sensor, lv_color_hex(0x323232), 0);
        lv_obj_set_style_shadow_width(sidebar_btn_sensor, 0, LV_PART_MAIN);
        lv_obj_set_style_radius(sidebar_btn_sensor, LV_RADIUS_CIRCLE, LV_PART_MAIN);
        lv_obj_add_flag(sidebar_btn_sensor, LV_OBJ_FLAG_CLICKABLE);

        // lv_obj_set_style_bg_color(sidebar_btn_home, lv_color_hex(0x535353), LV_STATE_FOCUSED);

        lv_obj_add_event_cb(sidebar_btn_sensor, [](lv_event_t *e)
                            {
        auto *manager = (HomeAssistantDisplayManager*)lv_event_get_user_data(e);
        manager->setActiveFilter(EntityFilter::SENSOR); }, LV_EVENT_CLICKED, this);

        lv_obj_t *label_sensor = lv_label_create(sidebar_btn_sensor);
        lv_label_set_text(label_sensor, FA_ICON_SENSOR);
        lv_obj_set_style_text_color(label_sensor, lv_color_hex(0xFFFFFF), 0);
        lv_obj_set_style_text_font(label_sensor, &font_awesome_solid_20, 0);
        lv_obj_center(label_sensor);

        // HEADER
        lv_obj_t *header_container = lv_obj_create(screen_main);
        lv_obj_set_size(header_container, 404, 40);
        lv_obj_align(header_container, LV_ALIGN_TOP_RIGHT, 0, 0);
        lv_obj_set_style_bg_color(header_container, lv_color_hex(0x000000), 0);
        lv_obj_set_style_border_width(header_container, 0, 0);
        lv_obj_set_style_pad_all(header_container, 5, 0);
        lv_obj_set_flex_flow(header_container, LV_FLEX_FLOW_ROW);
        lv_obj_set_scrollbar_mode(header_container, LV_SCROLLBAR_MODE_OFF);

        header_list_label = lv_label_create(header_container);
        lv_label_set_text(header_list_label, LV_SYMBOL_HOME "  Home Assistant");
        lv_obj_set_style_text_color(header_list_label, lv_color_hex(0xFFFFFF), 0);
        lv_obj_set_style_text_font(header_list_label, &lv_font_montserrat_14, 0);
        lv_obj_align(header_list_label, LV_ALIGN_TOP_LEFT, 0, 0);
        lv_obj_set_style_pad_top(header_list_label, 8, 0);

        // Container vuoto
        content_container = lv_obj_create(screen_main);
        lv_obj_set_size(content_container, 410, 280);
        lv_obj_align(content_container, LV_ALIGN_TOP_RIGHT, 0, 40);
        lv_obj_set_style_bg_color(content_container, lv_color_hex(0x000000), 0);
        lv_obj_set_style_border_width(content_container, 0, 0);
        lv_obj_set_style_pad_all(content_container, 5, 0);
        // lv_obj_set_flex_flow(content_container, LV_FLEX_FLOW_COLUMN);
        // lv_obj_set_scrollbar_mode(content_container, LV_SCROLLBAR_MODE_AUTO);

        lv_obj_add_flag(content_container, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_remove_flag(content_container, LV_OBJ_FLAG_SCROLL_MOMENTUM);

        APP_LOGGER("✅ main screen created (empty)");
    }

    void HomeAssistantDisplayManager::resetContentContainer()
    {
        lv_obj_clean(content_container);
        lv_group_remove_all_objs(encoder_group);
        
        // Reset to default state
        lv_obj_set_flex_flow(content_container, LV_FLEX_FLOW_COLUMN);
        lv_obj_set_flex_align(content_container, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);
        lv_obj_set_scrollbar_mode(content_container, LV_SCROLLBAR_MODE_OFF);
        lv_obj_remove_flag(content_container, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_remove_flag(content_container, LV_OBJ_FLAG_SCROLL_MOMENTUM);
    }

    void HomeAssistantDisplayManager::renderLoading()
    {
        current_view = ViewType::LOADING;

        // Clean content
        resetContentContainer();

        // set alignment
        lv_obj_set_flex_align(content_container, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

        // clean header
        lv_label_set_text(header_list_label, "");

        // Spinner
        lv_obj_t *spinner_loading = lv_spinner_create(content_container);
        lv_obj_set_size(spinner_loading, 80, 80);
        lv_obj_align(spinner_loading, LV_ALIGN_CENTER, 0, -60);
        lv_obj_set_style_arc_color(spinner_loading, lv_color_hex(0x009ac7), LV_PART_INDICATOR);

        // Label
        lv_obj_t *label_loading = lv_label_create(content_container);
        lv_label_set_text(label_loading, "Syncronizing, please wait..");
        lv_obj_set_style_text_color(label_loading, lv_color_hex(0x656565), 0);
        lv_obj_align(label_loading, LV_ALIGN_CENTER, 0, 60);
    }

    void HomeAssistantDisplayManager::showEntityList()
    {
        lv_screen_load(screen_main);
        current_view = ViewType::ENTITY_LIST;
        renderEntityList();
    }

    // ============================================================================
    // SCREEN 3: ENTITY DETAIL (placeholder)
    // ============================================================================

    void HomeAssistantDisplayManager::renderEntityDetail(const String &entityId)
    {
        APP_LOGGER("Showing detail for entity %s", entityId);

        resetContentContainer();

        // // Clean content
        // lv_obj_clean(content_container);
        // lv_group_remove_all_objs(encoder_group);

        // // Set layout for list view
        // lv_obj_set_flex_flow(content_container, LV_FLEX_FLOW_COLUMN);
        // lv_obj_set_scrollbar_mode(content_container, LV_SCROLLBAR_MODE_AUTO);
        // lv_obj_add_flag(content_container, LV_OBJ_FLAG_SCROLLABLE);
        // lv_obj_remove_flag(content_container, LV_OBJ_FLAG_SCROLL_MOMENTUM);

        auto entity = AppStore::instance().getEntity(entityId);
        lv_label_set_text(header_list_label, entity->getFriendlyName());

        if (entityId.startsWith("switch."))
        {
            current_view = ViewType::SWITCH_DETAIL;
            renderSwitchDetail(entityId);
        }
        else if (entityId.startsWith("light."))
        {
            current_view = ViewType::LIGHT_DETAIL;
            renderLightDetail(entityId);
        }
        else if (entityId.startsWith("climate."))
        {
            current_view = ViewType::CLIMATE_DETAIL;
            renderClimateDetail(entityId);
        }
        else if (entityId.startsWith("sensor."))
        {
            current_view = ViewType::SENSOR_DETAIL;
            renderSensorDetail(entityId);
        }
        else if (entityId.startsWith("cover."))
        {
            current_view = ViewType::COVER_DETAIL;
            renderCoverDetail(entityId);
        }
    }

    void HomeAssistantDisplayManager::renderCoverDetail(const String &entityId)
    {
        lv_obj_t *btn_container = lv_obj_create(content_container);
        lv_obj_set_size(btn_container, 384, 260);
        lv_obj_align(btn_container, LV_ALIGN_CENTER, 0, 0);
        lv_obj_set_style_bg_color(btn_container, lv_color_hex(0x1a1a1a), 0);
        lv_obj_set_style_border_width(btn_container, 0, 0);
        lv_obj_set_scrollbar_mode(btn_container, LV_SCROLLBAR_MODE_OFF);
        lv_obj_set_flex_flow(btn_container, LV_FLEX_FLOW_ROW);
        lv_obj_set_flex_align(btn_container, LV_FLEX_ALIGN_SPACE_EVENLY, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

        cover_btn_up = lv_button_create(btn_container);
        lv_obj_set_size(cover_btn_up, 100, 60);
        lv_obj_set_style_bg_color(cover_btn_up, lv_color_hex(0x323232), 0);
        lv_obj_set_style_bg_color(cover_btn_up, lv_color_hex(0x525252), LV_STATE_FOCUSED);
        lv_obj_add_flag(cover_btn_up, LV_OBJ_FLAG_CLICKABLE);

        lv_obj_t *label_up = lv_label_create(cover_btn_up);
        lv_obj_set_style_text_font(label_up, &lv_font_montserrat_20, 0);
        lv_label_set_text(label_up, LV_SYMBOL_UP);
        lv_obj_center(label_up);

        cover_btn_off = lv_button_create(btn_container);
        lv_obj_set_size(cover_btn_off, 100, 60);
        lv_obj_set_style_bg_color(cover_btn_off, lv_color_hex(0x323232), 0);
        lv_obj_set_style_bg_color(cover_btn_off, lv_color_hex(0x525252), LV_STATE_FOCUSED);
        lv_obj_add_flag(cover_btn_off, LV_OBJ_FLAG_CLICKABLE);

        lv_obj_t *label_off = lv_label_create(cover_btn_off);
        lv_obj_set_style_text_font(label_off, &lv_font_montserrat_20, 0);
        lv_label_set_text(label_off, LV_SYMBOL_STOP);
        lv_obj_center(label_off);

        cover_btn_dwn = lv_button_create(btn_container);
        lv_obj_set_size(cover_btn_dwn, 100, 60);
        lv_obj_set_style_bg_color(cover_btn_dwn, lv_color_hex(0x323232), 0);
        lv_obj_set_style_bg_color(cover_btn_dwn, lv_color_hex(0x525252), LV_STATE_FOCUSED);
        lv_obj_add_flag(cover_btn_dwn, LV_OBJ_FLAG_CLICKABLE);

        lv_obj_t *label_dwn = lv_label_create(cover_btn_dwn);
        lv_obj_set_style_text_font(label_dwn, &lv_font_montserrat_20, 0);
        lv_label_set_text(label_dwn, LV_SYMBOL_DOWN);
        lv_obj_center(label_dwn);

        lv_group_add_obj(encoder_group, cover_btn_up);
        lv_group_add_obj(encoder_group, cover_btn_off);
        lv_group_add_obj(encoder_group, cover_btn_dwn);

        APP_LOGGER("✅ Cover detail screen created");
    }

    void HomeAssistantDisplayManager::renderClimateDetail(const String &entityId)
    {
        auto entity = AppStore::instance().getEntity(entityId);

        lv_label_set_text(header_label, entity->getFriendlyName());

        float target = entity->getAttribute("temperature");
        int current = entity->getAttribute("current_temperature");
        const char *state = entity->getAttribute("hvac_action");

        // set currentTargetValue to be managed by ENCODER_ROTATIONS events
        currentTargetValue = target * 10;

        int integer = target * 10;
        int parte_intera = integer / 10;
        int parte_decimale = integer % 10;

        climate_arc_editing = false;

        // Container principale
        lv_obj_t *container = lv_obj_create(content_container);
        lv_obj_set_size(container, 384, 260);
        lv_obj_align(container, LV_ALIGN_CENTER, 0, 0);
        lv_obj_set_style_bg_color(container, lv_color_hex(0x1a1a1a), 0);
        lv_obj_set_style_border_width(container, 0, 0);
        lv_obj_set_scrollbar_mode(container, LV_SCROLLBAR_MODE_OFF);

        lv_obj_t *arc_container = lv_obj_create(container);
        lv_obj_set_size(arc_container, 250, 260);
        lv_obj_align(arc_container, LV_ALIGN_LEFT_MID, 0, 10);
        lv_obj_set_style_bg_opa(arc_container, 0, 0);
        lv_obj_set_style_border_width(arc_container, 0, 0);
        lv_obj_set_flex_flow(arc_container, LV_FLEX_FLOW_ROW);
        lv_obj_set_flex_align(arc_container, LV_FLEX_ALIGN_SPACE_EVENLY, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
        lv_obj_set_scrollbar_mode(arc_container, LV_SCROLLBAR_MODE_OFF);

        // Arc slider (centro)
        climate_arc_slider = lv_arc_create(arc_container);
        lv_obj_set_size(climate_arc_slider, 210, 210);
        lv_obj_align(climate_arc_slider, LV_ALIGN_CENTER, 0, 0);
        lv_arc_set_range(climate_arc_slider, 150, 300);
        lv_arc_set_bg_angles(climate_arc_slider, 135, 45);
        lv_obj_set_style_arc_width(climate_arc_slider, 15, LV_PART_MAIN);
        lv_obj_set_style_arc_color(climate_arc_slider, lv_color_hex(0x292929), LV_PART_MAIN);
        lv_obj_set_style_arc_width(climate_arc_slider, 18, LV_PART_INDICATOR);
        lv_obj_set_style_arc_color(climate_arc_slider, lv_color_hex(0xFF6F22), LV_PART_INDICATOR);

        // Style per focus
        lv_obj_set_style_arc_color(climate_arc_slider, lv_color_hex(0xDD6626), LV_PART_INDICATOR | LV_STATE_FOCUSED);
        lv_obj_set_style_arc_width(climate_arc_slider, 18, LV_PART_INDICATOR | LV_STATE_FOCUSED);
        lv_obj_set_style_arc_color(climate_arc_slider, lv_color_hex(0x8D421A), LV_PART_INDICATOR | LV_STATE_EDITED);
        lv_obj_add_flag(climate_arc_slider, LV_OBJ_FLAG_CLICKABLE);

        // Label target temperatura (dentro l'arco)
        climate_label_target = lv_label_create(climate_arc_slider);
        lv_obj_set_style_text_font(climate_label_target, &lv_font_montserrat_48, 0);
        lv_obj_set_style_text_color(climate_label_target, lv_color_hex(0xFFFFFF), 0);
        lv_obj_align(climate_label_target, LV_ALIGN_CENTER, -8, 0);

        climate_label_target_unit = lv_label_create(climate_arc_slider);
        lv_label_set_text(climate_label_target_unit, "°C");
        lv_obj_align(climate_label_target_unit, LV_ALIGN_CENTER, 28, -13);
        lv_obj_set_style_text_font(climate_label_target_unit, &lv_font_montserrat_14, 0);
        lv_obj_set_style_text_color(climate_label_target_unit, lv_color_hex(0xFFFFFF), 0);
        // lv_obj_center(climate_label_target_unit);

        climate_label_target_decimal = lv_label_create(climate_arc_slider);
        lv_obj_align(climate_label_target_decimal, LV_ALIGN_CENTER, 28, 13);
        lv_obj_set_style_text_font(climate_label_target_decimal, &lv_font_montserrat_14, 0);
        lv_obj_set_style_text_color(climate_label_target_decimal, lv_color_hex(0xFFFFFF), 0);
        // lv_obj_center(climate_label_target_decimal);

        // Label stato (sopra l'arco)
        climate_label_state = lv_label_create(climate_arc_slider);
        lv_obj_set_style_text_font(climate_label_state, &lv_font_montserrat_16, 0);
        lv_obj_set_style_text_color(climate_label_state, lv_color_hex(0x888888), 0);
        lv_obj_align(climate_label_state, LV_ALIGN_TOP_MID, 0, 50);

        // Label temperatura corrente (sotto l'arco)
        climate_label_current = lv_label_create(climate_arc_slider);
        lv_obj_set_style_text_font(climate_label_current, &lv_font_montserrat_18, 0);
        lv_obj_set_style_text_color(climate_label_current, lv_color_hex(0x999999), 0);
        lv_obj_align(climate_label_current, LV_ALIGN_BOTTOM_MID, 0, -45);

        // Bottoni ON/OFF (in basso)
        lv_obj_t *btn_container = lv_obj_create(container);
        lv_obj_set_size(btn_container, 115, 200);
        lv_obj_align(btn_container, LV_ALIGN_RIGHT_MID, 0, 0);
        lv_obj_set_style_bg_opa(btn_container, 0, 0);
        lv_obj_set_style_border_width(btn_container, 0, 0);
        lv_obj_set_flex_flow(btn_container, LV_FLEX_FLOW_COLUMN_WRAP);
        lv_obj_set_flex_align(btn_container, LV_FLEX_ALIGN_SPACE_AROUND, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
        lv_obj_set_scrollbar_mode(btn_container, LV_SCROLLBAR_MODE_OFF);

        // Bottone ON
        climate_btn_on = lv_button_create(btn_container);
        lv_obj_set_size(climate_btn_on, 80, 60);
        lv_obj_set_style_bg_color(climate_btn_on, lv_color_hex(0x323232), 0);
        lv_obj_set_style_bg_color(climate_btn_on, lv_color_hex(0x525252), LV_STATE_FOCUSED);
        lv_obj_add_flag(climate_btn_on, LV_OBJ_FLAG_CLICKABLE);

        lv_obj_t *label_on = lv_label_create(climate_btn_on);
        lv_label_set_text(label_on, LV_SYMBOL_POWER " ON");
        lv_obj_center(label_on);

        // Bottone OFF
        climate_btn_off = lv_button_create(btn_container);
        lv_obj_set_size(climate_btn_off, 80, 60);
        lv_obj_set_style_bg_color(climate_btn_off, lv_color_hex(0x323232), 0);
        lv_obj_set_style_bg_color(climate_btn_off, lv_color_hex(0x525252), LV_STATE_FOCUSED);
        lv_obj_add_flag(climate_btn_off, LV_OBJ_FLAG_CLICKABLE);

        lv_obj_t *label_off = lv_label_create(climate_btn_off);
        lv_label_set_text(label_off, LV_SYMBOL_POWER " OFF");
        lv_obj_center(label_off);

        // data hidration
        lv_arc_set_value(climate_arc_slider, currentTargetValue);
        lv_label_set_text_fmt(climate_label_target, "%d", parte_intera);
        lv_label_set_text_fmt(climate_label_target_decimal, ".%d", parte_decimale);
        lv_label_set_text(climate_label_state, state);
        lv_label_set_text_fmt(climate_label_current, "%d°C", current);

        // Aggiungi al gruppo encoder
        lv_group_add_obj(encoder_group, climate_arc_slider);
        lv_group_add_obj(encoder_group, climate_btn_on);
        lv_group_add_obj(encoder_group, climate_btn_off);
        lv_group_focus_obj(climate_btn_on);

        APP_LOGGER("✅ Climate detail screen created");
    }

    void HomeAssistantDisplayManager::renderSensorDetail(const String &entityId)
    {
        auto entity = AppStore::instance().getEntity(entityId);

        lv_obj_t *status_container = lv_obj_create(content_container);
        lv_obj_set_size(status_container, 384, 260);
        lv_obj_align(status_container, LV_ALIGN_BOTTOM_MID, 0, -80);
        lv_obj_set_style_bg_color(status_container, lv_color_hex(0x1a1a1a), 0);
        lv_obj_set_style_border_width(status_container, 0, 0);
        lv_obj_set_flex_flow(status_container, LV_FLEX_FLOW_ROW);
        lv_obj_set_flex_align(status_container, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

        sensor_status_icon = lv_label_create(status_container);
        const char *unit = entity->getAttribute("unit_of_measurement");
        lv_label_set_text_fmt(sensor_status_icon, "%s %s", entity->getState(), unit);
        lv_obj_set_style_text_font(sensor_status_icon, &lv_font_montserrat_48, 0);
        lv_obj_set_style_text_color(sensor_status_icon, lv_color_hex(0xffffff), 0);
        lv_obj_center(sensor_status_icon);

        APP_LOGGER("✅ Sensor detail screen created");
    }

    void HomeAssistantDisplayManager::renderSwitchDetail(const String &entityId)
    {
        lv_obj_t *status_container = lv_obj_create(content_container);
        lv_obj_set_size(status_container, 384, 160);
        lv_obj_align(status_container, LV_ALIGN_BOTTOM_MID, 0, -80);
        lv_obj_set_style_bg_color(status_container, lv_color_hex(0x1a1a1a), 0);
        lv_obj_set_style_border_width(status_container, 0, 0);
        lv_obj_set_flex_flow(status_container, LV_FLEX_FLOW_ROW);
        lv_obj_set_flex_align(status_container, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

        switch_status_icon = lv_label_create(status_container);
        lv_label_set_text(switch_status_icon, LV_SYMBOL_CHARGE);
        lv_obj_set_style_text_font(switch_status_icon, &lv_font_montserrat_48, 0);

        // Container placeholder
        lv_obj_t *button_container = lv_obj_create(content_container);
        lv_obj_set_size(button_container, 384, 100);
        lv_obj_align(button_container, LV_ALIGN_BOTTOM_MID, 0, 0);
        lv_obj_set_style_bg_color(button_container, lv_color_hex(0x1a1a1a), 0);
        lv_obj_set_style_border_width(button_container, 0, 0);
        lv_obj_set_flex_flow(button_container, LV_FLEX_FLOW_ROW);
        lv_obj_set_flex_align(button_container, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

        // Bottone ON
        switch_btn_on = lv_button_create(button_container);
        lv_obj_set_size(switch_btn_on, 100, 60);
        lv_obj_set_style_bg_color(switch_btn_on, lv_color_hex(0x323232), 0);
        lv_obj_set_style_shadow_width(switch_btn_on, 0, 0);
        lv_obj_set_style_bg_color(switch_btn_on, lv_color_hex(0x525252), LV_STATE_FOCUSED);
        lv_obj_add_flag(switch_btn_on, LV_OBJ_FLAG_CLICKABLE);

        lv_obj_t *label_on = lv_label_create(switch_btn_on);
        lv_label_set_text(label_on, LV_SYMBOL_POWER " ON");
        lv_obj_center(label_on);

        // Bottone OFF
        switch_btn_off = lv_button_create(button_container);
        lv_obj_set_size(switch_btn_off, 100, 60);
        lv_obj_set_style_bg_color(switch_btn_off, lv_color_hex(0x323232), 0);
        lv_obj_set_style_shadow_width(switch_btn_off, 0, 0);
        lv_obj_set_style_bg_color(switch_btn_off, lv_color_hex(0x525252), LV_STATE_FOCUSED);
        lv_obj_add_flag(switch_btn_off, LV_OBJ_FLAG_CLICKABLE);

        lv_obj_t *label_off = lv_label_create(switch_btn_off);
        lv_label_set_text(label_off, LV_SYMBOL_POWER " OFF");
        lv_obj_center(label_off);

        // Aggiungi al gruppo encoder
        lv_group_add_obj(encoder_group, switch_btn_on);
        lv_group_add_obj(encoder_group, switch_btn_off);

        auto entity = AppStore::instance().getEntity(entityId);

        if (String(entity->getState()).equals("on"))
        {
            lv_obj_set_style_text_color(switch_status_icon, lv_color_hex(0xffc107), 0);
            lv_obj_add_state(switch_btn_on, LV_STATE_DISABLED);
            lv_obj_remove_state(switch_btn_off, LV_STATE_DISABLED);
            lv_group_focus_obj(switch_btn_off);
        }
        else
        {
            lv_obj_set_style_text_color(switch_status_icon, lv_color_hex(0x6f757a), 0);
            lv_obj_add_state(switch_btn_off, LV_STATE_DISABLED);
            lv_obj_remove_state(switch_btn_on, LV_STATE_DISABLED);
            lv_group_focus_obj(switch_btn_on);
        }

        APP_LOGGER("✅ Switch detail screen created");
    }

    void HomeAssistantDisplayManager::renderLightDetail(const String &entityId)
    {
        lv_obj_t *status_container = lv_obj_create(content_container);
        lv_obj_set_size(status_container, 384, 160);
        lv_obj_align(status_container, LV_ALIGN_BOTTOM_MID, 0, -80);
        lv_obj_set_style_bg_color(status_container, lv_color_hex(0x1a1a1a), 0);
        lv_obj_set_style_border_width(status_container, 0, 0);
        lv_obj_set_flex_flow(status_container, LV_FLEX_FLOW_ROW);
        lv_obj_set_flex_align(status_container, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

        light_status_icon = lv_label_create(status_container);
        lv_label_set_text(light_status_icon, LV_SYMBOL_CHARGE);
        lv_obj_set_style_text_font(light_status_icon, &lv_font_montserrat_48, 0);

        // Container placeholder
        lv_obj_t *button_container = lv_obj_create(content_container);
        lv_obj_set_size(button_container, 384, 100);
        lv_obj_align(button_container, LV_ALIGN_BOTTOM_MID, 0, 0);
        lv_obj_set_style_bg_color(button_container, lv_color_hex(0x1a1a1a), 0);
        lv_obj_set_style_border_width(button_container, 0, 0);
        lv_obj_set_flex_flow(button_container, LV_FLEX_FLOW_ROW);
        lv_obj_set_flex_align(button_container, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

        // Bottone ON
        light_btn_on = lv_button_create(button_container);
        lv_obj_set_size(light_btn_on, 100, 60);
        lv_obj_set_style_bg_color(light_btn_on, lv_color_hex(0x323232), 0);
        lv_obj_set_style_shadow_width(light_btn_on, 0, 0);
        lv_obj_set_style_bg_color(light_btn_on, lv_color_hex(0x525252), LV_STATE_FOCUSED);
        lv_obj_add_flag(light_btn_on, LV_OBJ_FLAG_CLICKABLE);

        lv_obj_t *label_on = lv_label_create(light_btn_on);
        lv_label_set_text(label_on, LV_SYMBOL_POWER " ON");
        lv_obj_center(label_on);

        // Bottone OFF
        light_btn_off = lv_button_create(button_container);
        lv_obj_set_size(light_btn_off, 100, 60);
        lv_obj_set_style_bg_color(light_btn_off, lv_color_hex(0x323232), 0);
        lv_obj_set_style_shadow_width(light_btn_off, 0, 0);
        lv_obj_set_style_bg_color(light_btn_off, lv_color_hex(0x525252), LV_STATE_FOCUSED);
        lv_obj_add_flag(light_btn_off, LV_OBJ_FLAG_CLICKABLE);

        lv_obj_t *label_off = lv_label_create(light_btn_off);
        lv_label_set_text(label_off, LV_SYMBOL_POWER " OFF");
        lv_obj_center(label_off);

        // Aggiungi al gruppo encoder
        lv_group_add_obj(encoder_group, light_btn_on);
        lv_group_add_obj(encoder_group, light_btn_off);

        auto entity = AppStore::instance().getEntity(entityId);

        if (String(entity->getState()).equals("on"))
        {
            lv_obj_set_style_text_color(light_status_icon, lv_color_hex(0xffc107), 0);
            lv_obj_add_state(light_btn_on, LV_STATE_DISABLED);
            lv_obj_remove_state(light_btn_off, LV_STATE_DISABLED);
            lv_group_focus_obj(light_btn_off);
        }
        else
        {
            lv_obj_set_style_text_color(light_status_icon, lv_color_hex(0x6f757a), 0);
            lv_obj_add_state(light_btn_off, LV_STATE_DISABLED);
            lv_obj_remove_state(light_btn_on, LV_STATE_DISABLED);
            lv_group_focus_obj(light_btn_on);
        }

        APP_LOGGER("✅ Light detail screen created");
    }

    void HomeAssistantDisplayManager::showEntityDetail(const String &entityId)
    {
        APP_LOGGER("Showing detail for entity %s", entityId);

        renderEntityDetail(entityId);
    }

    void HomeAssistantDisplayManager::showLoading()
    {
        lv_screen_load(screen_main);
        renderLoading();
    }

    void HomeAssistantDisplayManager::focusSidebar()
    {
        APP_LOGGER("Switching focus to sidebar");

        lv_group_remove_all_objs(encoder_group);
        lv_group_add_obj(encoder_group, sidebar_btn_home);
        lv_group_add_obj(encoder_group, sidebar_btn_light);
        lv_group_add_obj(encoder_group, sidebar_btn_switch);
        lv_group_add_obj(encoder_group, sidebar_btn_clima);
        lv_group_add_obj(encoder_group, sidebar_btn_cover);
        lv_group_add_obj(encoder_group, sidebar_btn_sensor);
        lv_group_set_wrap(encoder_group, false);

        // Focus the currently active filter button
        switch (current_filter)
        {
        case EntityFilter::ALL:
            lv_group_focus_obj(sidebar_btn_home);
            break;
        case EntityFilter::LIGHT:
            lv_group_focus_obj(sidebar_btn_light);
            break;
        case EntityFilter::SWITCH:
            lv_group_focus_obj(sidebar_btn_switch);
            break;
        case EntityFilter::CLIMA:
            lv_group_focus_obj(sidebar_btn_clima);
            break;
        case EntityFilter::COVER:
            lv_group_focus_obj(sidebar_btn_cover);
            break;
        case EntityFilter::SENSOR:
            lv_group_focus_obj(sidebar_btn_sensor);
            break;
        }

        APP_LOGGER("✅ Sidebar focused");
    }

    void HomeAssistantDisplayManager::focusEntityList()
    {
        APP_LOGGER("Switching focus back to entity list");

        lv_group_remove_all_objs(encoder_group);

        // Re-add all entity items from container
        uint32_t child_count = lv_obj_get_child_count(content_container);
        for (uint32_t i = 0; i < child_count; i++)
        {
            lv_obj_t *child = lv_obj_get_child(content_container, i);
            if (lv_obj_has_flag(child, LV_OBJ_FLAG_CLICKABLE))
            {
                lv_group_add_obj(encoder_group, child);
            }
        }

        lv_group_set_wrap(encoder_group, false);

        // Focus first entity if any
        if (child_count > 0)
        {
            lv_obj_t *first_item = lv_obj_get_child(content_container, 0);
            if (lv_obj_has_flag(first_item, LV_OBJ_FLAG_CLICKABLE))
            {
                lv_group_focus_obj(first_item);
            }
        }

        APP_LOGGER("✅ Entity list focused");
    }

    void HomeAssistantDisplayManager::renderEntityList()
    {
        APP_LOGGER("Rendering entity list with filter: %d", (int)current_filter);

        resetContentContainer();        
        
        // sett scrollbar mode
        lv_obj_set_scrollbar_mode(content_container, LV_SCROLLBAR_MODE_AUTO);
        lv_obj_add_flag(content_container, LV_OBJ_FLAG_SCROLLABLE);

        // Update header
        updateHeaderLabel();

        // Get filter colors
        FilterColors colors = getFilterColors(current_filter);

        // Get entities from prefs
        String entitiesJson = prefs.getSelectedEntities();

        if (entitiesJson.isEmpty())
        {
            lv_obj_t *empty = lv_label_create(content_container);
            lv_label_set_text(empty, "No entities configured.\nPlease configure via web interface.");
            lv_obj_set_style_text_align(empty, LV_TEXT_ALIGN_CENTER, 0);
            lv_obj_set_style_text_color(empty, lv_color_hex(0xFFFFFF), 0);
            lv_obj_center(empty);
            return;
        }

        // Parse JSON
        JsonDocument doc;
        DeserializationError error = deserializeJson(doc, entitiesJson);

        if (error)
        {
            APP_LOGGER("❌ Failed to parse entities JSON: %s", error.c_str());
            return;
        }

        JsonArray entities = doc.as<JsonArray>();
        int entityCount = 0;
        int indexToFocus = -1;

        for (int i = 0; i < entities.size(); i++)
        {
            JsonObject entity = entities[i];
            String entityId = entity["entity_id"].as<String>();
            String friendlyName = entity["friendly_name"].as<String>();

            // Apply filter
            bool shouldShow = false;
            switch (current_filter)
            {
            case EntityFilter::ALL:
                shouldShow = true;
                break;
            case EntityFilter::LIGHT:
                shouldShow = entityId.startsWith("light.");
                break;
            case EntityFilter::SWITCH:
                shouldShow = entityId.startsWith("switch.");
                break;
            case EntityFilter::CLIMA:
                shouldShow = entityId.startsWith("climate.");
                break;
            case EntityFilter::COVER:
                shouldShow = entityId.startsWith("cover.");
                break;
            case EntityFilter::SENSOR:
                shouldShow = entityId.startsWith("sensor.");
                break;
            }

            if (!shouldShow)
                continue;

            auto entityData = AppStore::instance().getEntity(entityId);
            if (!entityData)
            {
                APP_LOGGER("⚠️ Entity not found in store: %s", entityId.c_str());
                continue;
            }

            if (friendlyName.isEmpty())
                friendlyName = entityId;

            // Create item
            lv_obj_t *item = lv_obj_create(content_container);
            lv_obj_set_size(item, 384, 46);

            // Styling
            lv_obj_set_style_bg_color(item, lv_color_hex(0x1f2224), 0);
            lv_obj_set_style_border_width(item, 1, 0);
            lv_obj_set_style_border_color(item, lv_color_hex(0x1f2224), 0);
            lv_obj_set_style_radius(item, 8, 0);
            lv_obj_set_style_pad_ver(item, -5, 0);
            lv_obj_set_style_pad_hor(item, 10, 0);

            lv_obj_set_style_bg_color(item, lv_color_hex(colors.bg_color), LV_STATE_FOCUSED);
            lv_obj_set_style_border_color(item, lv_color_hex(colors.border_color), LV_STATE_FOCUSED);
            lv_obj_set_style_border_width(item, 1, LV_STATE_FOCUSED);

            lv_obj_add_flag(item, LV_OBJ_FLAG_CLICKABLE);
            lv_obj_clear_flag(item, LV_OBJ_FLAG_SCROLLABLE);

            lv_obj_add_event_cb(item, [](lv_event_t *e)
                                {
                lv_obj_t *target = lv_event_get_target_obj(e);
                lv_obj_scroll_to_view(target, LV_ANIM_ON); }, LV_EVENT_FOCUSED, NULL);

            // Label
            lv_obj_t *label = lv_label_create(item);
            lv_label_set_text(label, friendlyName.c_str());
            lv_obj_set_style_text_color(label, lv_color_hex(0xFFFFFF), 0);
            lv_obj_set_style_text_font(label, &lv_font_montserrat_14, 0);
            lv_obj_align(label, LV_ALIGN_LEFT_MID, 10, 0);

            // State LED for lights/switches
            const char *state = entityData->getState();

            if (entityId.startsWith("light.") || entityId.startsWith("switch."))
            {
                lv_obj_t *status_led = lv_obj_create(item);
                lv_obj_align(status_led, LV_ALIGN_RIGHT_MID, -45, 0);
                lv_obj_set_size(status_led, 13, 13);
                lv_obj_set_style_border_width(status_led, 0, 0);
                lv_obj_set_style_radius(status_led, LV_RADIUS_CIRCLE, 0);

                if (String(state).equals("on"))
                    lv_obj_set_style_bg_color(status_led, lv_color_hex(0xffc107), 0);
                else
                    lv_obj_set_style_bg_color(status_led, lv_color_hex(0x6f757a), 0);
            }

            // State label
            lv_obj_t *state_label = lv_label_create(item);
            lv_label_set_text(state_label, state);
            lv_obj_set_style_text_color(state_label, lv_color_hex(0xFFFFFF), 0);
            lv_obj_set_style_text_font(state_label, &lv_font_montserrat_12, 0);
            lv_obj_align(state_label, LV_ALIGN_RIGHT_MID, -10, 0);

            // Save entity_id
            lv_obj_set_user_data(item, strdup(entityId.c_str()));

            // Add to group
            lv_group_add_obj(encoder_group, item);

            // Track focus index
            if (!currentEntityId.isEmpty() && entityId.equals(currentEntityId))
            {
                indexToFocus = entityCount;
            }

            entityCount++;
        }

        // Handle empty results
        if (entityCount == 0)
        {
            String filterName;
            switch (current_filter)
            {
            case EntityFilter::ALL:
                filterName = "entities";
                break;
            case EntityFilter::LIGHT:
                filterName = "lights";
                break;
            case EntityFilter::SWITCH:
                filterName = "switches";
                break;
            case EntityFilter::CLIMA:
                filterName = "clima";
                break;
            case EntityFilter::COVER:
                filterName = "covers";
                break;
            case EntityFilter::SENSOR:
                filterName = "sensors";
                break;
            }

            lv_obj_t *empty = lv_label_create(content_container);
            lv_label_set_text_fmt(empty, "No %s found.", filterName.c_str());
            lv_obj_set_style_text_align(empty, LV_TEXT_ALIGN_CENTER, 0);
            lv_obj_set_style_text_color(empty, lv_color_hex(0xFFFFFF), 0);
            lv_obj_center(empty);
            return;
        }

        lv_group_set_wrap(encoder_group, false);

        // Focus correct item
        if (indexToFocus >= 0)
        {
            lv_obj_t *itemToFocus = lv_obj_get_child(content_container, indexToFocus);
            lv_group_focus_obj(itemToFocus);
            lv_obj_scroll_to_view(itemToFocus, LV_ANIM_OFF);
        }
        else if (entityCount > 0)
        {
            lv_obj_t *itemToFocus = lv_obj_get_child(content_container, 0);
            lv_group_focus_obj(itemToFocus);
        }

        APP_LOGGER("✅ Entity list rendered with %d entities", entityCount);
    }

    HomeAssistantDisplayManager::FilterColors HomeAssistantDisplayManager::getFilterColors(EntityFilter filter)
    {
        switch (filter)
        {
        case EntityFilter::ALL:
            return {0x0d6079, 0x0d6079}; // Home button colors

        case EntityFilter::LIGHT:
            return {0x8936ec, 0x8936ec}; // Light button colors

        case EntityFilter::SWITCH:
            return {0x4b57f8, 0x4b57f8}; // Switch button colors

        case EntityFilter::CLIMA:
            return {0xe37a0c, 0xe37a0c}; // Home button colors

        case EntityFilter::COVER:
            return {0x227c71, 0x227c71}; // Light button colors

        case EntityFilter::SENSOR:
            return {0xb8702d, 0xb8702d}; // Switch button colors

        default:
            return {0x0d6079, 0x0d6079};
        }
    }

    void HomeAssistantDisplayManager::updateSidebarStyles()
    {
        // Reset all to default (inactive)
        lv_obj_set_style_bg_color(sidebar_btn_home, lv_color_hex(0x323232), 0);
        lv_obj_set_style_bg_color(sidebar_btn_light, lv_color_hex(0x323232), 0);
        lv_obj_set_style_bg_color(sidebar_btn_switch, lv_color_hex(0x323232), 0);
        lv_obj_set_style_bg_color(sidebar_btn_clima, lv_color_hex(0x323232), 0);
        lv_obj_set_style_bg_color(sidebar_btn_cover, lv_color_hex(0x323232), 0);
        lv_obj_set_style_bg_color(sidebar_btn_sensor, lv_color_hex(0x323232), 0);

        // Set active filter color
        switch (current_filter)
        {
        case EntityFilter::ALL:
            lv_obj_set_style_bg_color(sidebar_btn_home, lv_color_hex(0x0d6079), 0);
            break;

        case EntityFilter::LIGHT:
            lv_obj_set_style_bg_color(sidebar_btn_light, lv_color_hex(0x8936ec), 0);
            break;

        case EntityFilter::SWITCH:
            lv_obj_set_style_bg_color(sidebar_btn_switch, lv_color_hex(0x4b57f8), 0);
            break;

        case EntityFilter::CLIMA:
            lv_obj_set_style_bg_color(sidebar_btn_clima, lv_color_hex(0xe37a0c), 0);
            break;

        case EntityFilter::COVER:
            lv_obj_set_style_bg_color(sidebar_btn_cover, lv_color_hex(0x227c71), 0);
            break;

        case EntityFilter::SENSOR:
            lv_obj_set_style_bg_color(sidebar_btn_sensor, lv_color_hex(0xb8702d), 0);
            break;
        }
    }

    // Add this new method
    void HomeAssistantDisplayManager::updateHeaderLabel()
    {
        switch (current_filter)
        {
        case EntityFilter::ALL:
            lv_label_set_text(header_list_label, /* LV_SYMBOL_HOME "   */ "Home Assistant");
            break;

        case EntityFilter::LIGHT:
            lv_label_set_text(header_list_label, /* FA_ICON_LIGHT "   */ "Lights");
            break;

        case EntityFilter::SWITCH:
            lv_label_set_text(header_list_label, /* FA_ICON_SWITCH "   */ "Switches");
            break;

        case EntityFilter::CLIMA:
            lv_label_set_text(header_list_label, /* FA_ICON_CLIMATE "   */ "Clima");
            break;

        case EntityFilter::COVER:
            lv_label_set_text(header_list_label, /* FA_ICON_COVER "   */ "Covers");
            break;

        case EntityFilter::SENSOR:
            lv_label_set_text(header_list_label, /* FA_ICON_SENSOR "   */ "Sensors");
            break;
        }

        APP_LOGGER("Header updated: %s", lv_label_get_text(header_list_label));
    }

    void HomeAssistantDisplayManager::setActiveFilter(EntityFilter filter)
    {
        current_filter = filter;
        updateSidebarStyles();
        updateHeaderLabel();
        renderEntityList(); // Re-populate with new filter
        focusEntityList();  // Return focus to entity list
    }

    // =========================================================
    // Helpers methods
    // =========================================================

    // Add this helper method to your HomeAssistantDisplayManager class

    void HomeAssistantDisplayManager::updateEntityItem(const String &entityId)
    {
        APP_LOGGER("Updating entity item: %s", entityId.c_str());

        // Get updated data from store
        auto entityData = AppStore::instance().getEntity(entityId);
        if (!entityData)
        {
            APP_LOGGER("⚠️ Entity not found in store: %s", entityId.c_str());
            return;
        }

        if (current_view == ViewType::CLIMATE_DETAIL)
        {

            float target = entityData->getAttribute("temperature");
            int current = entityData->getAttribute("current_temperature");
            const char *state = entityData->getAttribute("hvac_action");

            // set currentTargetValue to be managed by ENCODER_ROTATIONS events
            currentTargetValue = target * 10;

            int integer = target * 10;
            int parte_intera = integer / 10;
            int parte_decimale = integer % 10;

            climate_arc_editing = false;

            // TODO: fetch data from Home Assistant per entityId
            lv_arc_set_value(climate_arc_slider, currentTargetValue);
            lv_label_set_text_fmt(climate_label_target, "%d", parte_intera);
            lv_label_set_text_fmt(climate_label_target_decimal, ".%d", parte_decimale);
            lv_label_set_text(climate_label_state, state);
            lv_label_set_text_fmt(climate_label_current, "%d°C", current);
        }
        else if (current_view == ViewType::SWITCH_DETAIL)
        {
            if (String(entityData->getState()).equals("on"))
            {
                lv_obj_set_style_text_color(switch_status_icon, lv_color_hex(0xffc107), 0);
                lv_obj_add_state(switch_btn_on, LV_STATE_DISABLED);
                lv_obj_remove_state(switch_btn_off, LV_STATE_DISABLED);
                lv_group_focus_obj(switch_btn_off);
            }
            else
            {
                lv_obj_set_style_text_color(switch_status_icon, lv_color_hex(0x6f757a), 0);
                lv_obj_add_state(switch_btn_off, LV_STATE_DISABLED);
                lv_obj_remove_state(switch_btn_on, LV_STATE_DISABLED);
                lv_group_focus_obj(switch_btn_on);
            }
        }
        else if (current_view == ViewType::LIGHT_DETAIL)
        {
            if (String(entityData->getState()).equals("on"))
            {
                APP_LOGGER("LIGHT IS ON");
                lv_obj_set_style_text_color(light_status_icon, lv_color_hex(0xffc107), 0);
                lv_obj_add_state(light_btn_on, LV_STATE_DISABLED);
                lv_obj_remove_state(light_btn_off, LV_STATE_DISABLED);
                lv_group_focus_obj(light_btn_off);
            }
            else
            {
                APP_LOGGER("LIGHT IS OFF");
                lv_obj_set_style_text_color(light_status_icon, lv_color_hex(0x6f757a), 0);
                lv_obj_add_state(light_btn_off, LV_STATE_DISABLED);
                lv_obj_remove_state(light_btn_on, LV_STATE_DISABLED);
                lv_group_focus_obj(light_btn_on);
            }
        }
        else if (current_view == ViewType::ENTITY_LIST)
        {
            // Find the item in the list by iterating children
            uint32_t child_count = lv_obj_get_child_count(content_container);

            for (uint32_t i = 0; i < child_count; i++)
            {
                lv_obj_t *item = lv_obj_get_child(content_container, i);

                // Check if this is the right item by comparing user_data
                const char *stored_id = (const char *)lv_obj_get_user_data(item);
                if (stored_id && entityId.equals(stored_id))
                {
                    // Found it! Update the state label
                    updateStateLabel(item, entityData);
                    APP_LOGGER("✅ Updated entity item: %s", entityId.c_str());
                    return;
                }
            }

            APP_LOGGER("⚠️ Entity item not found in list: %s", entityId.c_str());
        }
    }

    // Helper to update just the state label
    void HomeAssistantDisplayManager::updateStateLabel(lv_obj_t *item, std::shared_ptr<HomeAssistantEntity> entityData)
    {
        // Find the state label (it's the second child, aligned right)
        uint32_t child_count = lv_obj_get_child_count(item);

        const char *stored_id = (const char *)lv_obj_get_user_data(item);

        if (String(stored_id).startsWith("light.") || String(stored_id).startsWith("switch."))
        {
            const char *state = entityData->getState();

            lv_obj_t *state_led = lv_obj_get_child(item, 1);
            lv_obj_t *state_label = lv_obj_get_child(item, 2); // Second child

            if (state)
            {
                lv_label_set_text(state_label, state);

                if (String(state).equals("on"))
                {
                    lv_obj_set_style_bg_color(state_led, lv_color_hex(0xffc107), 0);
                }
                else
                {
                    lv_obj_set_style_bg_color(state_led, lv_color_hex(0x6f757a), 0);
                }
            }
        }
        else if (child_count >= 2)
        {
            lv_obj_t *state_label = lv_obj_get_child(item, 1); // Second child
            const char *state = entityData->getState();

            if (state)
            {
                lv_label_set_text(state_label, state);
            }
        }
    }

}