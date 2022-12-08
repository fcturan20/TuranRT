
#include "scene.h"
#include <assimp/postprocess.h>
#include <assimp/scene.h>

#include <assimp/Importer.hpp>
#include <vector>

#include "ecs_tapi.h"
#include "resourceManager.h"

struct scene_rt {
  entityTypeHnd_ecstapi          defaultEntityType;
  std::vector<entityHnd_ecstapi> entities;
};

entityHnd_ecstapi rtSceneModifier::addDefaultEntity(rtScene scene) { return nullptr; }

void rtSceneModifier::createEntitiesWithAssimp(aiNode* rootNode) {}

rtScene rtSceneManager::createScene() { return nullptr; }
bool    rtSceneManager::destroyScene() { return false; }

struct rtSceneManager_private {
  static bool                  deserializeScene(rtResourceDesc* desc) { return false; }
  static bool                  isSceneValid(void* dataHnd) { return false; }
  static rtResourceManagerType managerType;
};
rtResourceManagerType rtSceneManager_private::managerType = {};

rtResourceManagerType rtSceneManager::managerType() { return rtSceneManager_private::managerType; }

void rtSceneManager::initializeManager() {
  rtResourceManager::managerDesc desc;
  desc.managerName                    = "Default Scene Resource Manager";
  desc.managerVer                     = MAKE_PLUGIN_VERSION_TAPI(0, 0, 0);
  desc.deserialize                    = rtSceneManager_private::deserializeScene;
  desc.validate                       = rtSceneManager_private::isSceneValid;
  rtSceneManager_private::managerType = rtResourceManager::registerManager(desc);
}