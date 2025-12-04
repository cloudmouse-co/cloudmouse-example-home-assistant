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

        createHomeScreen();
        // createMainScreen();
        createLoadingScreen();
        createConfigNeededScreen();
        createEntityListScreen();
        createEntityDetailScreen();
        createClimateDetailScreen();
        createSwitchDetailScreen();

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
            populateEntityList();
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
            if (lv_screen_active() == screen_entity_list)
            {
                focusSidebar();
            }
            else if (lv_screen_active() == screen_entity_detail ||
                     lv_screen_active() == screen_switch_detail ||
                     lv_screen_active() == screen_climate_detail)
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
        case CloudMouse::EventType::ENCODER_CLICK:
        {
            APP_LOGGER("ENCODER CLICK");

            if (lv_screen_active() == screen_entity_list)
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

                        // Check entity type from entity_id
                        if (String(entityId).startsWith("climate."))
                        {
                            APP_LOGGER("Opening climate detail");
                            showClimateDetail(String(entityId));
                        }
                        else if (String(entityId).startsWith("switch."))
                        {
                            APP_LOGGER("Opening switch detail");
                            showSwitchDetail(String(entityId));
                        }
                        else
                        {
                            APP_LOGGER("Opening generic detail");

                            // If showEntityDetail REALLY needs an index, find it:
                            // int realIndex = findEntityIndexById(entityId);
                            showEntityDetail(1);
                        }
                    }
                }
            }
            else if (lv_screen_active() == screen_climate_detail)
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
            else if (lv_screen_active() == screen_switch_detail)
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
            else if (lv_screen_active() == screen_entity_detail)
            {
                showEntityList();
            }

            break;
        }

        case CloudMouse::EventType::ENCODER_ROTATION:
        {
            if (lv_screen_active() == screen_climate_detail)
            {
                // APP_LOGGER("CLIMATE ARC EDITING IS: %s", climate_arc_editing ? "true" : "false");

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

                    // APP_LOGGER("Arc value: %.1f", newValue / 10.0f);
                }
            }
            break;
        }

        case CloudMouse::EventType::ENCODER_LONG_PRESS:
        {
            APP_LOGGER("ENCODER LONG PRESS");

            if (lv_screen_active() == screen_climate_detail && climate_arc_editing)
            {
                break;
            }

            // Long press torna sempre alla lista da qualsiasi detail
            // if (lv_screen_active() == screen_climate_detail ||
            //     lv_screen_active() == screen_entity_detail)
            // {
            climate_arc_editing = false; // reset editing mode
            showEntityList();
            // }
            break;
        }

        default:
            break;
        }
    }

    void HomeAssistantDisplayManager::createHomeScreen()
    {
        screen_home = lv_obj_create(NULL);
        lv_obj_set_style_bg_color(screen_home, lv_color_hex(0x000000), 0);

        // Header
        createHeader(screen_home, "Home Assistant");

        // Grid container
        lv_obj_t *grid = lv_obj_create(screen_home);
        lv_obj_set_size(grid, 460, 240);
        lv_obj_align(grid, LV_ALIGN_CENTER, 0, 20);
        lv_obj_set_style_bg_color(grid, lv_color_hex(0x252d2f), 0);
        lv_obj_set_style_border_width(grid, 0, 0);
        lv_obj_set_style_pad_all(grid, 10, 0);
        lv_obj_set_layout(grid, LV_LAYOUT_GRID);

        int32_t col_dsc[] = {220, 220, LV_GRID_TEMPLATE_LAST};
        int32_t row_dsc[] = {110, 110, LV_GRID_TEMPLATE_LAST};
        lv_obj_set_grid_dsc_array(grid, col_dsc, row_dsc);

        APP_LOGGER("✅ Home screen created");
    }

    void HomeAssistantDisplayManager::createHeader(lv_obj_t *parent, const char *title)
    {
        lv_obj_t *header = lv_obj_create(parent);
        lv_obj_set_size(header, 480, 40);
        lv_obj_align(header, LV_ALIGN_TOP_MID, 0, 0);
        lv_obj_set_style_bg_color(header, lv_color_hex(0x222222), 0);
        lv_obj_set_style_border_width(header, 0, 0);
        lv_obj_set_style_radius(header, 0, 0);
        lv_obj_set_scrollbar_mode(header, LV_SCROLLBAR_MODE_OFF);

        header_label = lv_label_create(header);
        lv_label_set_text(header_label, title);
        lv_obj_set_style_text_color(header_label, lv_color_hex(0xFFFFFF), 0);
        lv_obj_center(header_label);
    }

    void HomeAssistantDisplayManager::createLoadingScreen()
    {
        screen_loading = lv_obj_create(NULL);
        lv_obj_set_style_bg_color(screen_loading, lv_color_hex(0x1a1a1a), 0);

        // Spinner
        lv_obj_t *spinner_loading = lv_spinner_create(screen_loading);
        lv_obj_set_size(spinner_loading, 80, 80);
        lv_obj_center(spinner_loading);
        lv_obj_set_style_arc_color(spinner_loading, lv_color_hex(0x009ac7), LV_PART_INDICATOR);

        // Label
        lv_obj_t *label_loading = lv_label_create(screen_loading);
        lv_label_set_text(label_loading, "Synchronizing with Home Assistant...");
        lv_obj_set_style_text_color(label_loading, lv_color_hex(0xcccccc), 0);
        lv_obj_align(label_loading, LV_ALIGN_CENTER, 0, 80);

        Serial.println("✅ LOADING screen created");
    }

    // ============================================================================
    // SCREEN 1: CONFIG NEEDED (con QR code)
    // ============================================================================

    void HomeAssistantDisplayManager::createConfigNeededScreen()
    {
        screen_config_needed = lv_obj_create(NULL);
        lv_obj_set_style_bg_color(screen_config_needed, lv_color_hex(0x000000), 0);

        createHeader(screen_config_needed, "Configuration Needed");

        // Container principale
        lv_obj_t *container = lv_obj_create(screen_config_needed);
        lv_obj_set_size(container, 460, 260);
        lv_obj_align(container, LV_ALIGN_CENTER, 0, 10);
        lv_obj_set_style_bg_color(container, lv_color_hex(0x111111), 0);
        lv_obj_set_style_border_width(container, 2, 0);
        lv_obj_set_style_border_color(container, lv_color_hex(0xFF9800), 0);
        lv_obj_set_style_pad_all(container, 20, 0);
        lv_obj_set_flex_flow(container, LV_FLEX_FLOW_COLUMN);
        lv_obj_set_flex_align(container, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

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
        lv_obj_set_style_bg_color(screen_entity_list, lv_color_hex(0x000000), 0);

        // createHeader(screen_entity_list, "Entities");

        // SIDEBAR
        lv_obj_t *sidebar_container = lv_obj_create(screen_entity_list);
        lv_obj_set_size(sidebar_container, 76, 320);
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
        lv_obj_t *header_container = lv_obj_create(screen_entity_list);
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
        content_container = lv_obj_create(screen_entity_list);
        lv_obj_set_size(content_container, 404, 280);
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

    void HomeAssistantDisplayManager::createEntityListScreen()
    {
        screen_entity_list = lv_obj_create(NULL);
        lv_obj_set_style_bg_color(screen_entity_list, lv_color_hex(0x000000), 0);

        // createHeader(screen_entity_list, "Entities");

        // SIDEBAR
        lv_obj_t *sidebar_container = lv_obj_create(screen_entity_list);
        lv_obj_set_size(sidebar_container, 76, 320);
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
        lv_obj_t *header_container = lv_obj_create(screen_entity_list);
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
        entity_list_container = lv_obj_create(screen_entity_list);
        lv_obj_set_size(entity_list_container, 404, 280);
        lv_obj_align(entity_list_container, LV_ALIGN_TOP_RIGHT, 0, 40);
        lv_obj_set_style_bg_color(entity_list_container, lv_color_hex(0x000000), 0);
        lv_obj_set_style_border_width(entity_list_container, 0, 0);
        lv_obj_set_style_pad_all(entity_list_container, 5, 0);
        lv_obj_set_flex_flow(entity_list_container, LV_FLEX_FLOW_COLUMN);
        lv_obj_set_scrollbar_mode(entity_list_container, LV_SCROLLBAR_MODE_AUTO);

        lv_obj_add_flag(entity_list_container, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_remove_flag(entity_list_container, LV_OBJ_FLAG_SCROLL_MOMENTUM);

        APP_LOGGER("✅ Entity list screen created (empty)");
    }

    void HomeAssistantDisplayManager::populateEntityList()
    {
        APP_LOGGER("Populating entity list with filter: %d", (int)current_filter);

        lv_obj_clean(entity_list_container);
        lv_group_remove_all_objs(encoder_group);

        // Get filter colors
        FilterColors colors = getFilterColors(current_filter);

        // Get entities from prefs
        String entitiesJson = prefs.getSelectedEntities();

        if (entitiesJson.isEmpty())
        {
            APP_LOGGER("⚠️ No entities configured");
            lv_obj_t *empty = lv_label_create(entity_list_container);
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
        int entityCount = 0; // Count filtered entities

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
            {
                continue; // Skip this entity
            }

            entityCount++;

            auto entityData = AppStore::instance().getEntity(entityId);
            if (!entityData)
            {
                APP_LOGGER("⚠️ Entity not found in store: %s", entityId.c_str());
                continue;
            }

            // Fallback if friendly_name empty
            if (friendlyName.isEmpty())
            {
                friendlyName = entityId;
            }

            lv_obj_t *item = lv_obj_create(entity_list_container);
            lv_obj_set_size(item, 384, 46);
            lv_obj_align(item, LV_ALIGN_LEFT_MID, 15, 0);

            // Normal state
            lv_obj_set_style_bg_color(item, lv_color_hex(0x1f2224), 0);
            lv_obj_set_style_border_width(item, 1, 0);
            lv_obj_set_style_border_color(item, lv_color_hex(0x1f2224), 0);
            lv_obj_set_style_radius(item, 8, 0);
            lv_obj_set_style_pad_ver(item, -5, 0);
            lv_obj_set_style_pad_hor(item, 10, 0);

            // FOCUSED state - Use filter colors!
            lv_obj_set_style_bg_color(item, lv_color_hex(colors.bg_color), LV_STATE_FOCUSED);
            lv_obj_set_style_border_color(item, lv_color_hex(colors.border_color), LV_STATE_FOCUSED);
            lv_obj_set_style_border_width(item, 1, LV_STATE_FOCUSED);

            lv_obj_add_flag(item, LV_OBJ_FLAG_CLICKABLE);
            lv_obj_clear_flag(item, LV_OBJ_FLAG_SCROLLABLE);

            // Scroll when focused
            lv_obj_add_event_cb(item, [](lv_event_t *e)
                                {
            lv_obj_t *target = lv_event_get_target_obj(e);
            lv_obj_scroll_to_view(target, LV_ANIM_ON); }, LV_EVENT_FOCUSED, NULL);

            // Label with friendly name
            lv_obj_t *label = lv_label_create(item);
            lv_label_set_text(label, friendlyName.c_str());
            lv_obj_set_style_text_color(label, lv_color_hex(0xFFFFFF), 0);
            lv_obj_set_style_text_font(label, &lv_font_montserrat_14, 0);
            lv_obj_align(label, LV_ALIGN_LEFT_MID, 10, 0);

            const char *state = entityData->getState();

            if (entityId.startsWith("light."))
            {
                lv_obj_t *status_led = lv_obj_create(item);
                lv_obj_align(status_led, LV_ALIGN_RIGHT_MID, -45, 0);
                lv_obj_set_size(status_led, 13, 13);
                lv_obj_set_style_border_width(status_led, 0, 0);
                lv_obj_set_style_radius(status_led, LV_RADIUS_CIRCLE, 0);

                if (String(state).equals("on"))
                {
                    lv_obj_set_style_bg_color(status_led, lv_color_hex(0xffc107), 0);
                }
                else
                {
                    lv_obj_set_style_bg_color(status_led, lv_color_hex(0x6f757a), 0);
                }
            }
            else if (entityId.startsWith("switch."))
            {
                lv_obj_t *status_led = lv_obj_create(item);
                lv_obj_align(status_led, LV_ALIGN_RIGHT_MID, -45, 0);
                lv_obj_set_size(status_led, 13, 13);
                lv_obj_set_style_border_width(status_led, 0, 0);
                lv_obj_set_style_radius(status_led, LV_RADIUS_CIRCLE, 0);

                if (String(state).equals("on"))
                {
                    lv_obj_set_style_bg_color(status_led, lv_color_hex(0xffc107), 0);
                }
                else
                {
                    lv_obj_set_style_bg_color(status_led, lv_color_hex(0x6f757a), 0);
                }
            }

            lv_obj_t *state_label = lv_label_create(item);
            lv_label_set_text(state_label, state);
            lv_obj_set_style_text_color(state_label, lv_color_hex(0xFFFFFF), 0);
            lv_obj_set_style_text_font(state_label, &lv_font_montserrat_12, 0);
            lv_obj_align(state_label, LV_ALIGN_RIGHT_MID, -10, 0);

            // Save entity_id as user_data
            lv_obj_set_user_data(item, strdup(entityId.c_str()));

            lv_group_add_obj(encoder_group, item);
        }

        // Check if filter returned no entities
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

            APP_LOGGER("⚠️ No %s found", filterName.c_str());

            lv_obj_t *empty = lv_label_create(entity_list_container);
            lv_label_set_text_fmt(empty, "No %s found.", filterName.c_str());
            lv_obj_set_style_text_align(empty, LV_TEXT_ALIGN_CENTER, 0);
            lv_obj_set_style_text_color(empty, lv_color_hex(0xFFFFFF), 0);
            lv_obj_center(empty);
            return;
        }

        lv_group_set_wrap(encoder_group, false);

        APP_LOGGER("✅ Entity list populated with %d filtered entities", entityCount);
    }

    void HomeAssistantDisplayManager::showEntityList()
    {
        // Pulisci gruppo
        lv_group_remove_all_objs(encoder_group);

        int indexToFocus = -1;

        // Re-aggiungi tutti gli item esistenti e trova quello da focalizzare
        int count = lv_obj_get_child_count(entity_list_container);
        for (int i = 0; i < count; i++)
        {
            lv_obj_t *item = lv_obj_get_child(entity_list_container, i);
            lv_group_add_obj(encoder_group, item);

            // Trova l'indice dell'entity corrente
            if (!currentEntityId.isEmpty())
            {
                const char *entity_id = (const char *)lv_obj_get_user_data(item);
                if (String(entity_id).equals(currentEntityId))
                {
                    indexToFocus = i;
                }
            }
        }

        // Focus sull'elemento trovato o sul primo
        if (indexToFocus >= 0)
        {
            lv_obj_t *itemToFocus = lv_obj_get_child(entity_list_container, indexToFocus);
            lv_group_focus_obj(itemToFocus);
        }
        else if (count > 0)
        {
            lv_obj_t *itemToFocus = lv_obj_get_child(entity_list_container, 0);
            lv_group_focus_obj(itemToFocus);
        }

        lv_screen_load(screen_entity_list);

        APP_LOGGER("✅ Entity list shown");
    }

    // ============================================================================
    // SCREEN 3: ENTITY DETAIL (placeholder)
    // ============================================================================

    void HomeAssistantDisplayManager::createEntityDetailScreen()
    {
        screen_entity_detail = lv_obj_create(NULL);
        lv_obj_set_style_bg_color(screen_entity_detail, lv_color_hex(0x000000), 0);

        createHeader(screen_entity_detail, "Entity Detail");

        // Container placeholder
        lv_obj_t *container = lv_obj_create(screen_entity_detail);
        lv_obj_set_size(container, 460, 260);
        lv_obj_align(container, LV_ALIGN_CENTER, 0, 10);
        lv_obj_set_style_bg_color(container, lv_color_hex(0x1a1a1a), 0);
        lv_obj_set_style_border_width(container, 2, 0);
        lv_obj_set_style_border_color(container, lv_color_hex(0x2196F3), 0);
        lv_obj_set_flex_flow(container, LV_FLEX_FLOW_COLUMN);
        lv_obj_set_flex_align(container, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

        lv_obj_t *placeholder = lv_label_create(container);
        lv_label_set_text(placeholder, LV_SYMBOL_EDIT "\n\nEntity Detail\n\nComing Soon...");
        lv_obj_set_style_text_align(placeholder, LV_TEXT_ALIGN_CENTER, 0);
        lv_obj_set_style_text_font(placeholder, &lv_font_montserrat_24, 0);
        lv_obj_set_style_text_color(placeholder, lv_color_hex(0x888888), 0);

        APP_LOGGER("✅ Entity detail screen created");
    }

    void HomeAssistantDisplayManager::showEntityDetail(int entityIndex)
    {
        APP_LOGGER("Showing detail for entity %d", entityIndex);

        // TODO: qui metteremo logica per mostrare detail diversi per tipo entità

        lv_screen_load(screen_entity_detail);
    }

    void HomeAssistantDisplayManager::createSwitchDetailScreen()
    {
        screen_switch_detail = lv_obj_create(NULL);
        lv_obj_set_style_bg_color(screen_switch_detail, lv_color_hex(0x000000), 0);

        createHeader(screen_switch_detail, "Entity Sensor Detail");

        lv_obj_t *status_container = lv_obj_create(screen_switch_detail);
        lv_obj_set_size(status_container, 480, 180);
        lv_obj_align(status_container, LV_ALIGN_BOTTOM_MID, 0, -90);
        lv_obj_set_style_bg_color(status_container, lv_color_hex(0x1a1a1a), 0);
        lv_obj_set_style_border_width(status_container, 0, 0);
        lv_obj_set_flex_flow(status_container, LV_FLEX_FLOW_ROW);
        lv_obj_set_flex_align(status_container, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

        switch_status_icon = lv_label_create(status_container);
        lv_label_set_text(switch_status_icon, LV_SYMBOL_CHARGE);
        lv_obj_set_style_text_font(switch_status_icon, &lv_font_montserrat_48, 0);

        // Container placeholder
        lv_obj_t *button_container = lv_obj_create(screen_switch_detail);
        lv_obj_set_size(button_container, 480, 100);
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

        APP_LOGGER("✅ Sensor detail screen created");
    }

    void HomeAssistantDisplayManager::showSwitchDetail(const String &entityId)
    {
        APP_LOGGER("Showing detail for entity %s", entityId);

        auto entity = AppStore::instance().getEntity(entityId);
        lv_label_set_text(header_label, entity->getFriendlyName());

        lv_group_remove_all_objs(encoder_group);
        lv_group_add_obj(encoder_group, switch_btn_on);
        lv_group_add_obj(encoder_group, switch_btn_off);

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

        lv_screen_load(screen_switch_detail);
    }

    void HomeAssistantDisplayManager::createClimateDetailScreen()
    {
        screen_climate_detail = lv_obj_create(NULL);
        lv_obj_set_style_bg_color(screen_climate_detail, lv_color_hex(0x000000), 0);

        createHeader(screen_climate_detail, "Thermostat");

        // Container principale
        lv_obj_t *container = lv_obj_create(screen_climate_detail);
        lv_obj_set_size(container, 480, 260);
        lv_obj_align(container, LV_ALIGN_CENTER, 0, 15);
        lv_obj_set_style_bg_color(container, lv_color_hex(0x1a1a1a), 0);
        lv_obj_set_style_border_width(container, 0, 0);
        lv_obj_set_scrollbar_mode(container, LV_SCROLLBAR_MODE_OFF);

        lv_obj_t *arc_container = lv_obj_create(container);
        lv_obj_set_size(arc_container, 240, 260);
        lv_obj_align(arc_container, LV_ALIGN_LEFT_MID, 28, 10);
        lv_obj_set_style_bg_opa(arc_container, 0, 0);
        lv_obj_set_style_border_width(arc_container, 0, 0);
        lv_obj_set_flex_flow(arc_container, LV_FLEX_FLOW_ROW);
        lv_obj_set_flex_align(arc_container, LV_FLEX_ALIGN_SPACE_EVENLY, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
        lv_obj_set_scrollbar_mode(arc_container, LV_SCROLLBAR_MODE_OFF);

        // Arc slider (centro)
        climate_arc_slider = lv_arc_create(arc_container);
        lv_obj_set_size(climate_arc_slider, 210, 210);
        lv_obj_align(climate_arc_slider, LV_ALIGN_LEFT_MID, 55, -10);
        lv_arc_set_range(climate_arc_slider, 150, 300);
        lv_arc_set_value(climate_arc_slider, 210);
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
        lv_label_set_text(climate_label_target, "21");
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
        lv_label_set_text(climate_label_target_decimal, ",2");
        lv_obj_align(climate_label_target_decimal, LV_ALIGN_CENTER, 28, 13);
        lv_obj_set_style_text_font(climate_label_target_decimal, &lv_font_montserrat_14, 0);
        lv_obj_set_style_text_color(climate_label_target_decimal, lv_color_hex(0xFFFFFF), 0);
        // lv_obj_center(climate_label_target_decimal);

        // Label stato (sopra l'arco)
        climate_label_state = lv_label_create(climate_arc_slider);
        lv_label_set_text(climate_label_state, "Idle");
        lv_obj_set_style_text_font(climate_label_state, &lv_font_montserrat_16, 0);
        lv_obj_set_style_text_color(climate_label_state, lv_color_hex(0x888888), 0);
        lv_obj_align(climate_label_state, LV_ALIGN_TOP_MID, 0, 50);

        // Label temperatura corrente (sotto l'arco)
        climate_label_current = lv_label_create(climate_arc_slider);
        lv_label_set_text(climate_label_current, "22°C");
        lv_obj_set_style_text_font(climate_label_current, &lv_font_montserrat_18, 0);
        lv_obj_set_style_text_color(climate_label_current, lv_color_hex(0x999999), 0);
        lv_obj_align(climate_label_current, LV_ALIGN_BOTTOM_MID, 0, -45);

        // Bottoni ON/OFF (in basso)
        lv_obj_t *btn_container = lv_obj_create(container);
        lv_obj_set_size(btn_container, 140, 260);
        lv_obj_align(btn_container, LV_ALIGN_RIGHT_MID, 0, 0);
        lv_obj_set_style_bg_opa(btn_container, 0, 0);
        lv_obj_set_style_border_width(btn_container, 0, 0);
        lv_obj_set_flex_flow(btn_container, LV_FLEX_FLOW_COLUMN_WRAP);
        lv_obj_set_flex_align(btn_container, LV_FLEX_ALIGN_SPACE_EVENLY, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
        lv_obj_set_scrollbar_mode(btn_container, LV_SCROLLBAR_MODE_OFF);

        // Bottone ON
        climate_btn_on = lv_button_create(btn_container);
        lv_obj_set_size(climate_btn_on, 100, 100);
        lv_obj_set_style_bg_color(climate_btn_on, lv_color_hex(0x323232), 0);
        lv_obj_set_style_bg_color(climate_btn_on, lv_color_hex(0x525252), LV_STATE_FOCUSED);
        lv_obj_add_flag(climate_btn_on, LV_OBJ_FLAG_CLICKABLE);

        lv_obj_t *label_on = lv_label_create(climate_btn_on);
        lv_label_set_text(label_on, LV_SYMBOL_POWER " ON");
        lv_obj_center(label_on);

        // Bottone OFF
        climate_btn_off = lv_button_create(btn_container);
        lv_obj_set_size(climate_btn_off, 100, 100);
        lv_obj_set_style_bg_color(climate_btn_off, lv_color_hex(0x323232), 0);
        lv_obj_set_style_bg_color(climate_btn_off, lv_color_hex(0x525252), LV_STATE_FOCUSED);
        lv_obj_add_flag(climate_btn_off, LV_OBJ_FLAG_CLICKABLE);

        lv_obj_t *label_off = lv_label_create(climate_btn_off);
        lv_label_set_text(label_off, LV_SYMBOL_POWER " OFF");
        lv_obj_center(label_off);

        // Aggiungi al gruppo encoder
        lv_group_add_obj(encoder_group, climate_arc_slider);
        lv_group_add_obj(encoder_group, climate_btn_on);
        lv_group_add_obj(encoder_group, climate_btn_off);

        APP_LOGGER("✅ Climate detail screen created");
    }

    void HomeAssistantDisplayManager::showClimateDetail(const String &entityId)
    {
        APP_LOGGER("Showing climate detail for: %s", entityId.c_str());

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

        // Pulisci gruppo e ri-aggiungi solo gli oggetti di questa screen
        lv_group_remove_all_objs(encoder_group);
        lv_group_add_obj(encoder_group, climate_arc_slider);
        lv_group_add_obj(encoder_group, climate_btn_on);
        lv_group_add_obj(encoder_group, climate_btn_off);

        // TODO: fetch data from Home Assistant per entityId
        lv_arc_set_value(climate_arc_slider, currentTargetValue);
        lv_label_set_text_fmt(climate_label_target, "%d", parte_intera);
        lv_label_set_text_fmt(climate_label_target_decimal, ".%d", parte_decimale);
        lv_label_set_text(climate_label_state, state);
        lv_label_set_text_fmt(climate_label_current, "%d°C", current);

        lv_screen_load(screen_climate_detail);

        lv_group_focus_obj(climate_btn_on);
        lv_obj_remove_state(climate_arc_slider, LV_STATE_EDITED);
        lv_group_focus_obj(climate_btn_on);

        lv_group_focus_obj(NULL);
    }

    void HomeAssistantDisplayManager::showLoading()
    {
        lv_screen_load(screen_loading);
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
        uint32_t child_count = lv_obj_get_child_count(entity_list_container);
        for (uint32_t i = 0; i < child_count; i++)
        {
            lv_obj_t *child = lv_obj_get_child(entity_list_container, i);
            if (lv_obj_has_flag(child, LV_OBJ_FLAG_CLICKABLE))
            {
                lv_group_add_obj(encoder_group, child);
            }
        }

        lv_group_set_wrap(encoder_group, false);

        // Focus first entity if any
        if (child_count > 0)
        {
            lv_obj_t *first_item = lv_obj_get_child(entity_list_container, 0);
            if (lv_obj_has_flag(first_item, LV_OBJ_FLAG_CLICKABLE))
            {
                lv_group_focus_obj(first_item);
            }
        }

        APP_LOGGER("✅ Entity list focused");
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
            return {0x0ce3d9, 0x0ce3d9}; // Switch button colors

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
            lv_obj_set_style_bg_color(sidebar_btn_sensor, lv_color_hex(0x0ce3d9), 0);
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
        populateEntityList(); // Re-populate with new filter
        focusEntityList();    // Return focus to entity list
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

        if (lv_screen_active() == screen_climate_detail)
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
        else if (lv_screen_active() == screen_switch_detail)
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

        // Find the item in the list by iterating children
        uint32_t child_count = lv_obj_get_child_count(entity_list_container);

        for (uint32_t i = 0; i < child_count; i++)
        {
            lv_obj_t *item = lv_obj_get_child(entity_list_container, i);

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