#include <Geode/Geode.hpp>
#include <Geode/modify/PlayLayer.hpp>
#include <Geode/modify/PlayerObject.hpp>
#include <vector>
#include <algorithm>
#include <sstream>
#include <unordered_set>

using namespace geode::prelude;

std::unordered_set<int> g_removedObjects;

std::vector<int> parseCustomIDs(const std::string& ids_str) {
    std::vector<int> ids;
    std::stringstream ss(ids_str);
    std::string item;
    while (std::getline(ss, item, ',')) {
        try {
            item.erase(std::remove_if(item.begin(), item.end(), isspace), item.end());
            if (!item.empty()) {
                ids.push_back(std::stoi(item));
            }
        } catch (const std::exception& e) {
            log::warn("Не удалось обработать ID из списка: {}", item);
        }
    }
    return ids;
}

std::vector<int> getHazardObjectIDs() {
    std::vector<int> ids = {
        3, 10, 11, 41, 42, 43, 44, 85, 103, 111, 133,
        12, 13, 76, 114, 140
    };
    auto customIDs = parseCustomIDs(Mod::get()->getSettingValue<std::string>("custom-ids"));
    ids.insert(ids.end(), customIDs.begin(), customIDs.end());
    return ids;
}

bool isHazard(GameObject* obj, const std::vector<int>& hazardIDs) {
    return std::find(hazardIDs.begin(), hazardIDs.end(), obj->m_objectID) != hazardIDs.end();
}


class $modify(MyPlayLayer, PlayLayer) {
    bool init(GJGameLevel* level, bool useSecondMusic, bool p2) {
        if (!PlayLayer::init(level, useSecondMusic, p2)) {
            return false;
        }
        g_removedObjects.clear();
        log::info("Новый уровень, список удаленных объектов очищен.");
        return true;
    }
    void postUpdate(float dt) {
        PlayLayer::postUpdate(dt);
        for (auto* object : geode::cocos::CCArrayExt<GameObject*>(this->m_objects)) {
            if (g_removedObjects.count(object->m_uniqueID)) {
                object->setVisible(false);
            }
        }
    }
};

class $modify(MyPlayerObject, PlayerObject) {
    void playerDestroyed(bool p0) {
        PlayerObject::playerDestroyed(p0);

        auto playLayer = PlayLayer::get();
        if (!playLayer) return;

        if (!Mod::get()->getSettingValue<bool>("enabled")) return;

        const auto allHazardIDs = getHazardObjectIDs();
        auto potentialObjects = CCArray::create();
        const CCPoint deathPos = this->getPosition();
        const double removalRadius = Mod::get()->getSettingValue<double>("removal-radius");
        const float blockSize = 30.f;
        const float radiusSquared = (removalRadius * blockSize) * (removalRadius * blockSize);

        for (auto* object : geode::cocos::CCArrayExt<GameObject*>(playLayer->m_objects)) {
            if (isHazard(object, allHazardIDs) && g_removedObjects.find(object->m_uniqueID) == g_removedObjects.end()) {
                if (removalRadius > 0) {
                    if (object->getPosition().getDistanceSq(deathPos) > radiusSquared) {
                        continue;
                    }
                }
                potentialObjects->addObject(object);
            }
        }

        if (potentialObjects->count() == 0) {
            log::info("В радиусе смерти нет объектов для удаления.");
            return;
        }
        
        const int removeCount = Mod::get()->getSettingValue<long>("remove-count");
        const long removalMode = Mod::get()->getSettingValue<long>("removal-mode");
        const bool showParticles = Mod::get()->getSettingValue<bool>("show-particles");

        if (removalMode == 1) {
            std::sort(geode::cocos::CCArrayExt<GameObject*>(potentialObjects).begin(), geode::cocos::CCArrayExt<GameObject*>(potentialObjects).end(), [deathPos](GameObject* a, GameObject* b) {
                return a->getPosition().getDistanceSq(deathPos) < b->getPosition().getDistanceSq(deathPos);
            });
        } else if (removalMode == 2) {
             std::sort(geode::cocos::CCArrayExt<GameObject*>(potentialObjects).begin(), geode::cocos::CCArrayExt<GameObject*>(potentialObjects).end(), [](GameObject* a, GameObject* b) {
                return a->getPosition().x < b->getPosition().x;
            });
        } else if (removalMode == 3) {
             std::sort(geode::cocos::CCArrayExt<GameObject*>(potentialObjects).begin(), geode::cocos::CCArrayExt<GameObject*>(potentialObjects).end(), [](GameObject* a, GameObject* b) {
                return a->getPosition().x > b->getPosition().x;
            });
        }

        for (int i = 0; i < removeCount && potentialObjects->count() > 0; ++i) {
            GameObject* objToRemove = nullptr;

            if (removalMode == 0) {
                int randomIndex = rand() % potentialObjects->count();
                objToRemove = static_cast<GameObject*>(potentialObjects->objectAtIndex(randomIndex));
            } else {
                objToRemove = static_cast<GameObject*>(potentialObjects->objectAtIndex(0));
            }

            if (objToRemove) {
                log::info("Удаляется объект ID: {}, UID: {}", objToRemove->m_objectID, objToRemove->m_uniqueID);
                g_removedObjects.insert(objToRemove->m_uniqueID);
    
                if (showParticles) {
                    auto fullPath = CCFileUtils::sharedFileUtils()->fullPathForFilename("explode.plist", false);
                    auto dict = CCDictionary::createWithContentsOfFile(fullPath.c_str());

                    if (dict) {
                        auto particle = CCParticleSystemQuad::create();
                        if (particle && particle->initWithDictionary(dict, "")) {
                            particle->setPosition(objToRemove->getPosition());
                            particle->setAutoRemoveOnFinish(true);
                            playLayer->addChild(particle, 100);
                        }
                    }
                }
                
                potentialObjects->removeObject(objToRemove);
                objToRemove->setVisible(false);
            }
        }
    }
};
