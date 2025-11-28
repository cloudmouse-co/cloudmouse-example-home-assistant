#include "./HomeAssistantConfigServer.h"
#include "../core/EventBus.h"
#include "../HomeAssistantApp.h"
#include <WiFi.h>

namespace CloudMouse::App::Network
{

    HomeAssistantConfigServer *HomeAssistantConfigServer::instance = nullptr;

    HomeAssistantConfigServer::HomeAssistantConfigServer(HomeAssistantPrefs &preferences)
        : prefs(preferences)
    {
        instance = this;
    }

    HomeAssistantConfigServer::~HomeAssistantConfigServer()
    {
        instance = nullptr;
    }

    bool HomeAssistantConfigServer::init()
    {
        APP_LOGGER("Initializing Config server...");

        webServer = new WebServer(8080);

        if (!webServer)
        {
            APP_LOGGER("Failed to create WebServer");
            return false;
        }

        webServer->on("/home-assistant", HTTP_GET, handleSetupPage);
        webServer->on("/home-assistant/setup", HTTP_POST, handleSetupSubmit);
        webServer->on("/home-assistant/config", HTTP_GET, handleConfigPage);
        webServer->on("/home-assistant/config/save", HTTP_POST, handleConfigSubmit);

        webServer->on("/", HTTP_GET, []()
                      {
            instance->webServer->sendHeader("Location", "/home-assistant");
            instance->webServer->send(302); });

        webServer->begin();
        serverRunning = true;

        APP_LOGGER("Config Server started on port 8080");
        APP_LOGGER("Access at: http://%s:8080/home-assistant", WiFi.localIP().toString());

        if (MDNS.begin(mDNS.c_str()))
        {
            MDNS.addService("http", "tcp", 8080);
            APP_LOGGER("mDNS started: %s", getConfigURL().c_str());
        }

        return true;
    };

    String HomeAssistantConfigServer::getConfigURL()
    {
        return "http://" + mDNS + ".local:8080/home-assistant";
    }

    void HomeAssistantConfigServer::update()
    {
        if (webServer && serverRunning)
        {
            webServer->handleClient();
        }
    }

    bool HomeAssistantConfigServer::hasValidSetup()
    {
        return prefs.hasApiKey() && prefs.hasHost();
    }

    bool HomeAssistantConfigServer::hasValidConfig()
    {
        return prefs.hasSelectedEntities();
    }

    void HomeAssistantConfigServer::handleSetupPage()
    {
        if (!instance)
            return;

        if (instance->prefs.hasApiKey() && instance->prefs.hasHost())
        {
            instance->webServer->sendHeader("Location", "/home-assistant/config");
            instance->webServer->send(302);
            return;
        }

        APP_LOGGER("Serving setup page");

        String html = instance->generateSetupPage();
        instance->webServer->send(200, "text/html", html);
    }

    void HomeAssistantConfigServer::handleConfigPage()
    {
        if (!instance)
            return;

        if (!instance->prefs.hasApiKey() || !instance->prefs.hasHost())
        {
            instance->webServer->sendHeader("Location", "/home-assistant");
            instance->webServer->send(302);
            return;
        }

        APP_LOGGER("Serving config page");

        String html = instance->generateConfigPage();
        instance->webServer->send(200, "text/html", html);
    }

    void HomeAssistantConfigServer::handleSetupSubmit()
    {
        if (!instance)
            return;

        APP_LOGGER("Saving setup parameters");

        String apiKey = instance->webServer->arg("api_key");
        String host = instance->webServer->arg("host");

        instance->prefs.setApiKey(apiKey);
        instance->prefs.setHost(host);

        instance->webServer->sendHeader("Location", "/home-assistant/config");
        instance->webServer->send(302);

        CloudMouse::EventBus::instance().sendToUI(toSDKEvent(AppEventData::event(AppEventType::SETUP_SET)));
    }

    void HomeAssistantConfigServer::handleConfigSubmit()
    {
        if (!instance)
            return;

        if (instance->webServer->arg("action") == "reset") 
        {
            instance->prefs.resetConfiguration();

            instance->webServer->sendHeader("Location", "/home-assistant");
            instance->webServer->send(302);

            instance->configChangedCallback();
            return;
        }

        // Parse le entità dalla lista completa
        String fullEntitiesList = HomeAssistantDataService::fetchEntityList(instance->prefs);
        JsonDocument fullDoc;
        deserializeJson(fullDoc, fullEntitiesList);

        // Crea array con solo le entità selezionate (con friendly_name)
        JsonDocument selectedDoc;
        JsonArray selectedEntities = selectedDoc.to<JsonArray>();

        int argsCount = instance->webServer->args();
        for (int i = 0; i < argsCount; i++)
        {
            if (instance->webServer->argName(i) == "entities")
            {
                String entityId = instance->webServer->arg(i);

                // Trova il friendly_name dalla lista completa
                JsonArray allEntities = fullDoc.as<JsonArray>();
                for (JsonObject entity : allEntities)
                {
                    if (entity["entity_id"].as<String>() == entityId)
                    {
                        // Salva oggetto con entity_id E friendly_name
                        JsonObject selectedEntity = selectedEntities.add<JsonObject>();
                        selectedEntity["entity_id"] = entityId;
                        selectedEntity["friendly_name"] = entity["attributes"]["friendly_name"].as<String>();
                        selectedEntity["state"] = entity["state"].as<String>(); // bonus!
                        break;
                    }
                }
            }
        }

        // Salva JSON array di oggetti
        String entitiesJson;
        serializeJson(selectedDoc, entitiesJson);

        instance->prefs.setSelectedEntities(entitiesJson);

        APP_LOGGER("✅ Saved %d entities with names", selectedEntities.size());

        String html = "<!DOCTYPE html><html><head>";
        html += "<meta charset=\"UTF-8\">";
        html += "</head><body>";
        html += "<h1>Configuration Saved!</h1>";
        html += "<p>Saved " + String(selectedEntities.size()) + " entities</p>";
        html += "<a href=\"/home-assistant/config\">Back to configuration</a>";
        html += "</body></html>";

        instance->webServer->send(200, "text/html", html);

        instance->configChangedCallback();
    }

    String HomeAssistantConfigServer::generateSetupPage()
    {
        String html = "<!DOCTYPE html><html><head>";
        html += "<meta charset=\"UTF-8\">";
        html += "<meta name=\"viewport\" content=\"width=device-width, initial-scale=1.0\">";
        html += "<title>CloudMouse Home Assistant setup</title>";
        html += "<style>";
        // html += generateCSS();
        html += "</style></head><body>";
        html += "<div class=\"container\">";
        html += "<h1>Home Assistant setup</h1>";
        html += "<form method=\"POST\" action=\"/home-assistant/setup\">";
        html += "<label>Api Key <input name=\"api_key\" value=\"\" type=\"text\" /></label>";
        html += "<label>Home Assistant Host <input name=\"host\" value=\"\" type=\"text\" /></label>";
        html += "<input value=\"Save\" type=\"submit\" />";
        html += "</form>";
        html += "</div></body></html>";
        return html;
    }

    String HomeAssistantConfigServer::generateConfigPage()
    {
        String entitiesList = HomeAssistantDataService::fetchEntityList(prefs);

        if (entitiesList.startsWith("HTTP error"))
        {
            // Errore nella chiamata API
            return generateErrorPage(entitiesList);
        }

        // Parse selected entities from prefs
        String selectedJson = instance->prefs.getSelectedEntities();

        String html = "<!DOCTYPE html><html><head>";
        html += "<meta charset=\"UTF-8\">";
        html += "<meta name=\"viewport\" content=\"width=device-width, initial-scale=1.0\">";
        html += "<title>CloudMouse Home Assistant config</title>";
        html += "<style>";
        html += "body { font-family: Arial; margin: 20px; }";
        html += ".entity { padding: 8px; margin: 4px 0; background: #f0f0f0; border-radius: 4px; }";
        html += ".entity input { margin-right: 10px; }";
        html += ".entity-name { font-weight: bold; }";
        html += ".entity-state { color: #666; font-size: 0.9em; margin-left: 10px; }";
        html += "button { margin-top: 20px; padding: 10px 20px; background: #007bff; color: white; border: none; border-radius: 4px; cursor: pointer; }";
        html += "button:hover { background: #0056b3; }";
        html += "button.danger { margin-top: 20px; margin-left:15px; padding: 10px 20px; background: #df0000; color: white; border: none; border-radius: 4px; cursor: pointer; }";
        html += "button.danger:hover { background: #df0000; }";
        html += "</style></head><body>";
        html += "<div class=\"container\">";
        html += "<h1>Select Home Assistant Entities</h1>";
        html += "<form method=\"POST\" action=\"/home-assistant/config/save\">";

        // Parse entities and generate checkboxes
        html += generateEntityCheckboxes(entitiesList, selectedJson);

        html += "<button type=\"submit\" name=\"action\" value=\"save\">Save Configuration</button>";
        html += "<button type=\"submit\" name=\"action\" value=\"reset\" class=\"danger\">Reset Configuration</button>";
        html += "</form>";
        html += "</div></body></html>";

        return html;
    }

    String HomeAssistantConfigServer::generateEntityCheckboxes(const String &entitiesJson, const String &selectedJson)
    {
        String html = "";

        // Parse entities list
        JsonDocument doc;
        DeserializationError error = deserializeJson(doc, entitiesJson);

        if (error)
        {
            APP_LOGGER("❌ JSON parse error: %s", error.c_str());
            return "<p>Error parsing entities</p>";
        }

        // Parse selected entities (ORA SONO OGGETTI!)
        JsonDocument selectedDoc;
        if (!selectedJson.isEmpty())
        {
            deserializeJson(selectedDoc, selectedJson);
        }

        // Generate checkboxes for each entity
        JsonArray entities = doc.as<JsonArray>();

        for (JsonObject entity : entities)
        {
            String entityId = entity["entity_id"].as<String>();
            String state = entity["state"].as<String>();
            String friendlyName = entity["attributes"]["friendly_name"].as<String>();

            if (friendlyName.isEmpty())
            {
                friendlyName = entityId;
            }

            // Check if this entity is selected (CONTROLLA entity_id negli oggetti!)
            bool isSelected = false;
            if (!selectedJson.isEmpty() && selectedDoc.is<JsonArray>())
            {
                JsonArray selected = selectedDoc.as<JsonArray>();
                for (JsonObject selectedEntity : selected) // ✅ JsonObject invece di JsonVariant!
                {
                    if (selectedEntity["entity_id"].as<String>() == entityId)
                    {
                        isSelected = true;
                        break;
                    }
                }
            }

            html += "<div class=\"entity\">";
            html += "<input type=\"checkbox\" name=\"entities\" value=\"" + entityId + "\"";
            if (isSelected)
            {
                html += " checked";
            }
            html += ">";
            html += "<span class=\"entity-name\">" + friendlyName + "</span>";
            html += "<span class=\"entity-state\">(" + state + ")</span>";
            html += "</div>";
        }

        return html;
    }

    String HomeAssistantConfigServer::generateErrorPage(const String &error)
    {
        String html = "<!DOCTYPE html><html><head>";
        html += "<meta charset=\"UTF-8\">";
        html += "<title>Error</title>";
        html += "</head><body>";
        html += "<h1>Error loading entities</h1>";
        html += "<p>" + error + "</p>";
        html += "<form method=\"post\" action=\"/home-assistant/config/save\">";
        html += "<button type=\"submit\" name=\"action\" value=\"reset\">Go back to setup</a>";
        html += "</form>";
        html += "</body></html>";
        return html;
    }

} // namespace CloudMouse::App::Network