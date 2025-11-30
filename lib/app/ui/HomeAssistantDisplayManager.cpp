#include "HomeAssistantDisplayManager.h"
#include "../HomeAssistantApp.h"

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
        createConfigNeededScreen();
        createEntityListScreen();
        createEntityDetailScreen();
        createClimateDetailScreen();

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

                int count = lv_obj_get_child_count(entity_list_container);
                for (int i = 0; i < count; i++)
                {
                    if (lv_obj_get_child(entity_list_container, i) == focused)
                    {
                        selected_entity_index = i;

                        // Prendi le entità dalle prefs e trova quella selezionata
                        String entitiesJson = prefs.getSelectedEntities();
                        JsonDocument doc;
                        deserializeJson(doc, entitiesJson);
                        JsonArray entities = doc.as<JsonArray>();

                        if (i < entities.size())
                        {
                            JsonObject entity = entities[i];
                            String entityId = entity["entity_id"].as<String>();

                            APP_LOGGER("Selected entity: %s", entityId.c_str());

                            // Controlla il tipo dall'entity_id
                            if (entityId.startsWith("climate."))
                            {
                                APP_LOGGER("Opening climate detail");
                                showClimateDetail(entityId);
                            }
                            else
                            {
                                APP_LOGGER("Opening generic detail");
                                showEntityDetail(i);
                            }
                        }
                        break;
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
            else if (lv_screen_active() == screen_entity_detail)
            {
                showEntityList();
            }

            break;
        }

        case CloudMouse::EventType::ENCODER_ROTATION:
        {
            APP_LOGGER("ENCODER ROTATE (managed by LVGL): %d", event.value);

            APP_LOGGER("CLIMATE ARC EDITING IS: %s", climate_arc_editing ? "true" : "false");

            if (lv_screen_active() == screen_climate_detail)
            {
                lv_obj_t *focused = lv_group_get_focused(encoder_group);
                if (!focused) {
                    lv_group_focus_obj(climate_arc_slider);
                }

                if (climate_arc_editing) 
                {
                    int current = lv_arc_get_value(climate_arc_slider);
                    int newValue = current + event.value; // ✅ +1 o -1 = 0.1°C
    
                    // Clamp tra 150-300
                    if (newValue < 150)
                        newValue = 150;
                    if (newValue > 300)
                        newValue = 300;
    
                    lv_arc_set_value(climate_arc_slider, newValue);
    
                    // ✅ Mostra con decimale
                    float temp = newValue / 10.0f;
                    String tempStr = String(temp, 0);
                    int parte_intera = newValue / 10;
                    int parte_decimale = newValue % 10; 

                    lv_label_set_text_fmt(climate_label_target, "%d", parte_intera);
                    lv_label_set_text_fmt(climate_label_target_decimal, ".%d", parte_decimale);
    
                    APP_LOGGER("Arc value: %.1f", temp);
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
            if (lv_screen_active() == screen_climate_detail ||
                lv_screen_active() == screen_entity_detail)
            {
                climate_arc_editing = false; // reset editing mode
                showEntityList();
            }
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
        lv_obj_set_style_bg_color(grid, lv_color_hex(0x000000), 0);
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


        lv_obj_t *label = lv_label_create(header);
        lv_label_set_text(label, title);
        lv_obj_set_style_text_color(label, lv_color_hex(0xFFFFFF), 0);
        lv_obj_center(label);
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

    void HomeAssistantDisplayManager::createEntityListScreen()
    {
        screen_entity_list = lv_obj_create(NULL);
        lv_obj_set_style_bg_color(screen_entity_list, lv_color_hex(0x000000), 0);

        createHeader(screen_entity_list, "Entities");

        // Container vuoto
        entity_list_container = lv_obj_create(screen_entity_list);
        lv_obj_set_size(entity_list_container, 480, 260);
        lv_obj_align(entity_list_container, LV_ALIGN_CENTER, 0, 10);
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
        APP_LOGGER("Populating entity list from preferences");

        lv_obj_clean(entity_list_container);
        lv_group_remove_all_objs(encoder_group);

        // Prendi entità dalle prefs
        String entitiesJson = prefs.getSelectedEntities();

        if (entitiesJson.isEmpty())
        {
            APP_LOGGER("⚠️ No entities configured");
            // Mostra messaggio vuoto
            lv_obj_t *empty = lv_label_create(entity_list_container);
            lv_label_set_text(empty, "No entities configured.\nPlease configure via web interface.");
            lv_obj_set_style_text_align(empty, LV_TEXT_ALIGN_CENTER, 0);
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
        int entityCount = entities.size();

        if (entityCount == 0)
        {
            APP_LOGGER("⚠️ No entities selected");

            // Mostra messaggio vuoto
            lv_obj_t *empty = lv_label_create(entity_list_container);
            lv_label_set_text(empty, "No entities selected.\nPlease select via web interface.");
            lv_obj_set_style_text_align(empty, LV_TEXT_ALIGN_CENTER, 0);
            lv_obj_center(empty);
            return;
        }

        APP_LOGGER("Loading %d entities from preferences", entityCount);

        for (int i = 0; i < entityCount; i++)
        {
            JsonObject entity = entities[i];
            String entityId = entity["entity_id"].as<String>();
            String friendlyName = entity["friendly_name"].as<String>();

            // Fallback se friendly_name vuoto
            if (friendlyName.isEmpty())
            {
                friendlyName = entityId;
            }

            lv_obj_t *item = lv_obj_create(entity_list_container);
            lv_obj_set_size(item, LV_PCT(100), 60);

            // Normal state
            lv_obj_set_style_bg_color(item, lv_color_hex(0x2a2a2a), 0);
            lv_obj_set_style_border_width(item, 1, 0);
            lv_obj_set_style_border_color(item, lv_color_hex(0x3a3a3a), 0);
            lv_obj_set_style_radius(item, 8, 0);
            lv_obj_set_style_pad_all(item, 10, 0);

            // FOCUSED state
            lv_obj_set_style_bg_color(item, lv_color_hex(0x8d4119), LV_STATE_FOCUSED);
            lv_obj_set_style_border_color(item, lv_color_hex(0xFF6F22), LV_STATE_FOCUSED);
            lv_obj_set_style_border_width(item, 2, LV_STATE_FOCUSED);

            lv_obj_add_flag(item, LV_OBJ_FLAG_CLICKABLE);
            lv_obj_clear_flag(item, LV_OBJ_FLAG_SCROLLABLE);

            // Scroll quando focused
            lv_obj_add_event_cb(item, [](lv_event_t *e)
                                {
            lv_obj_t *target = lv_event_get_target_obj(e);
            lv_obj_scroll_to_view(target, LV_ANIM_ON); }, LV_EVENT_FOCUSED, NULL);

            // Label con friendly name
            lv_obj_t *label = lv_label_create(item);
            lv_label_set_text(label, friendlyName.c_str());
            lv_obj_set_style_text_color(label, lv_color_hex(0xFFFFFF), 0);
            lv_obj_set_style_text_font(label, &lv_font_montserrat_16, 0);
            lv_obj_align(label, LV_ALIGN_LEFT_MID, 5, 0);

            // Salva entity_id come user_data per usarlo dopo
            lv_obj_set_user_data(item, strdup(entityId.c_str()));

            lv_group_add_obj(encoder_group, item);
        }

        lv_group_set_wrap(encoder_group, false);

        APP_LOGGER("✅ Entity list populated with %d real entities", entityCount);
    }

    void HomeAssistantDisplayManager::showEntityList()
    {
        // Pulisci gruppo
        lv_group_remove_all_objs(encoder_group);

        // Re-aggiungi tutti gli item esistenti
        int count = lv_obj_get_child_count(entity_list_container);
        for (int i = 0; i < count; i++)
        {
            lv_obj_t *item = lv_obj_get_child(entity_list_container, i);
            lv_group_add_obj(encoder_group, item);
        }

        // ✅ Forza focus sul primo elemento (o sull'ultimo selezionato)
        if (count > 0 && selected_entity_index < count)
        {
            lv_obj_t *itemToFocus = lv_obj_get_child(entity_list_container, selected_entity_index);
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
        lv_obj_center(climate_label_target);

        climate_label_target_unit = lv_label_create(climate_arc_slider);
        lv_label_set_text(climate_label_target_unit, "°C");
        lv_obj_align(climate_label_target_unit, LV_ALIGN_CENTER, 36, -13);
        lv_obj_set_style_text_font(climate_label_target_unit, &lv_font_montserrat_14, 0);
        lv_obj_set_style_text_color(climate_label_target_unit, lv_color_hex(0xFFFFFF), 0);
        // lv_obj_center(climate_label_target_unit);

        climate_label_target_decimal = lv_label_create(climate_arc_slider);
        lv_label_set_text(climate_label_target_decimal, ",2");
        lv_obj_align(climate_label_target_decimal, LV_ALIGN_CENTER, 36, 13);
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

        climate_arc_editing = false;

        // Pulisci gruppo e ri-aggiungi solo gli oggetti di questa screen
        lv_group_remove_all_objs(encoder_group);
        lv_group_add_obj(encoder_group, climate_arc_slider);
        lv_group_add_obj(encoder_group, climate_btn_on);
        lv_group_add_obj(encoder_group, climate_btn_off);

        // TODO: fetch data from Home Assistant per entityId
        lv_arc_set_value(climate_arc_slider, 210);
        lv_label_set_text(climate_label_target, "21");
        lv_label_set_text(climate_label_state, "Idle");
        lv_label_set_text(climate_label_current, "22°C");
        
        lv_screen_load(screen_climate_detail);

        lv_group_focus_obj(climate_btn_on);
        lv_obj_remove_state(climate_arc_slider, LV_STATE_EDITED);
        lv_group_focus_obj(climate_btn_on);

        lv_group_focus_obj(NULL);
    }

}