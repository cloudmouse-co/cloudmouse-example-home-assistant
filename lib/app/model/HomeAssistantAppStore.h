// AppStore.h
#pragma once
#include <map>
#include <memory>
#include "HomeAssistantEntity.h"

namespace CloudMouse::App
{
    class AppStore
    {
    private:
        std::map<String, std::shared_ptr<HomeAssistantEntity>> entities;
        SemaphoreHandle_t mutex; // Critical for dual-core!

        AppStore()
        {
            mutex = xSemaphoreCreateMutex();
        }

    public:
        static AppStore &instance()
        {
            static AppStore instance;
            return instance;
        }

        // "Dispatch" - update state (Core 0 writes)
        void setEntity(const String &entityId, const String &payload)
        {
            auto entity = std::make_shared<HomeAssistantEntity>();

            if (entity->parse(payload))
            {
                xSemaphoreTake(mutex, portMAX_DELAY);
                entities[entityId] = entity;
                xSemaphoreGive(mutex);

                APP_LOGGER("Store updated: %s", entityId.c_str());
            }
        }

        // "Selector" - read state (Core 1 reads)
        std::shared_ptr<HomeAssistantEntity> getEntity(const String &entityId)
        {
            xSemaphoreTake(mutex, portMAX_DELAY);
            auto it = entities.find(entityId);
            auto result = (it != entities.end()) ? it->second : nullptr;
            xSemaphoreGive(mutex);
            return result;
        }

        // Get all entity IDs
        std::vector<String> getEntityIds()
        {
            std::vector<String> ids;
            xSemaphoreTake(mutex, portMAX_DELAY);
            for (auto &kv : entities)
            {
                ids.push_back(kv.first);
            }
            xSemaphoreGive(mutex);
            return ids;
        }
    };
}