#include <glm/glm.hpp>
#include <vector>

#include <tgfx_forwarddeclarations.h>
#include <tgfx_renderer.h>

#include "../editor_includes.h"
#include "ecs_tapi.h"
#include "resourceManager.h"
#include "mesh.h"
#include "shaderEffect.h"
#include "../render_context/rendercontext.h"
#include "../render_context/sceneRenderer.h"
#include "scene.h"

static const rtResourceManagerType* defmanagerType;
static struct tapi_ecs_entityType*  meshEntityType;
static tapi_ecs_componentTypeID*    meshCompTypeID;

struct rtScene {
  std::vector<struct tapi_ecs_entity*>    entities;
};
struct rtSceneManagerPrivate {
  static unsigned char deserializeScene(const rtResourceDesc* desc) { return false; }
  static unsigned char isSceneValid(void* dataHnd) { return false; }

  static const struct rtResourceManagerType* managerType() { return defmanagerType; }
  static tapi_ecs_component*                 createMeshComponent() {
    return ( tapi_ecs_component* )new rtMeshComponent;
  }
  static void destroyMeshComponent(tapi_ecs_component* comp) { delete ( rtMeshComponent* )comp; }
  static rtScene*                createScene() { return new rtScene; }
  static unsigned char           destroyScene(struct rtScene* scene) { return false; }
  static struct tapi_ecs_entity* addEntity(struct rtScene*             scene,
                                           struct tapi_ecs_entityType* type) {
    tapi_ecs_entity* ntt = editorECS->createEntity(type);
    scene->entities.push_back(ntt);
    return ntt;
  }
  static struct tapi_ecs_componentTypeID* getMeshComponentTypeID() { return meshCompTypeID; }
  static struct tapi_ecs_entityType* getMeshEntityType() { return meshEntityType; }
};

void initializeSceneManager() {
  // Set system struct
  {
    rtSceneManager* sManager           = new rtSceneManager;
    sManager->addEntity          = rtSceneManagerPrivate::addEntity;
    sManager->createScene              = rtSceneManagerPrivate::createScene;
    sManager->destroyScene             = rtSceneManagerPrivate::destroyScene;
    sManager->getMeshComponentTypeID   = rtSceneManagerPrivate::getMeshComponentTypeID;
    sManager->getMeshEntityType        = rtSceneManagerPrivate::getMeshEntityType;
    sManager->getResourceManagerType   = rtSceneManagerPrivate::managerType;
    sceneManager                       = sManager;
  }

  RM_managerDesc desc;
  desc.managerName = "Scene Resource Manager";
  desc.managerVer  = MAKE_PLUGIN_VERSION_TAPI(0, 0, 0);
  desc.deserialize = rtSceneManagerPrivate::deserializeScene;
  desc.validate    = rtSceneManagerPrivate::isSceneValid;
  defmanagerType   = RM_registerManager(desc);

  // Register RT Mesh Component
  {
    struct ecs_compManager compManager;
    compManager.createComponent  = rtSceneManagerPrivate::createMeshComponent;
    compManager.destroyComponent = rtSceneManagerPrivate::destroyMeshComponent;
    meshCompTypeID =
      editorECS->addComponentType("RT Mesh Component", nullptr, compManager, nullptr, 0);
    meshEntityType = editorECS->addEntityType(&meshCompTypeID, 1);
  }

  initializeSceneRenderer();
}