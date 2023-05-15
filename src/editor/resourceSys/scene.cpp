#include <glm/glm.hpp>
#include <vector>

#include <tgfx_forwarddeclarations.h>
#include <tgfx_renderer.h>

#include "../editor_includes.h"
#include "ecs_tapi.h"
#include "resourceManager.h"
#include "mesh.h"
#include "../render_context/rendercontext.h"
#include "scene.h"

struct rtSceneManager_private {
  static bool                  deserializeScene(rtResourceDesc* desc) { return false; }
  static bool                  isSceneValid(void* dataHnd) { return false; }
  static rtResourceManagerType managerType;
  static entityTypeHnd_ecstapi defaultEntityType;
  static compTypeID_ecstapi    defaultCompTypeID;
};
rtResourceManagerType rtSceneManager_private::managerType = {};
entityTypeHnd_ecstapi rtSceneManager_private::defaultEntityType;
compTypeID_ecstapi    rtSceneManager_private::defaultCompTypeID;

rtResourceManagerType sceneManager_rt::managerType() { return rtSceneManager_private::managerType; }

componentHnd_ecstapi createDefaultComp() { return (componentHnd_ecstapi) new defaultComponent_rt; }
void destroyDefaultComp(componentHnd_ecstapi comp) { delete ( defaultComponent_rt* )comp; }
void sceneManager_rt::initializeManager() {
  rtResourceManager::managerDesc desc;
  desc.managerName                    = "Default Scene Resource Manager";
  desc.managerVer                     = MAKE_PLUGIN_VERSION_TAPI(0, 0, 0);
  desc.deserialize                    = rtSceneManager_private::deserializeScene;
  desc.validate                       = rtSceneManager_private::isSceneValid;
  rtSceneManager_private::managerType = rtResourceManager::registerManager(desc);

  componentManager_ecs compManager;
  compManager.createComponent  = createDefaultComp;
  compManager.destroyComponent = destroyDefaultComp;
  rtSceneManager_private::defaultCompTypeID =
    editorECS->addComponentType("Simple Component", nullptr, compManager, nullptr, 0);
  rtSceneManager_private::defaultEntityType =
    editorECS->addEntityType(&rtSceneManager_private::defaultCompTypeID, 1);
}

struct scene_rt {
  std::vector<entityHnd_ecstapi>     entities;
  bindingTable_tgfxhnd               table;
  std::vector<commandBundle_tgfxhnd> rasterBndles;
  std::vector<commandBundle_tgfxhnd> computeBndles;
};

rtScene sceneManager_rt::createScene() { return new scene_rt; }
bool    sceneManager_rt::destroyScene() { return false; }

compType_ecstapi  compType = {};
entityHnd_ecstapi sceneModifier_rt::addDefaultEntity(rtScene scene) {
  entityHnd_ecstapi ntt = editorECS->createEntity(rtSceneManager_private::defaultEntityType);
  scene->entities.push_back(ntt);
  return ntt;
}

compTypeID_ecstapi defaultComponent_rt::getDefaultComponentTypeID() {
  return rtSceneManager_private::defaultCompTypeID;
}
void sceneModifier_rt::renderScene(rtScene scene) {
  scene->rasterBndles.clear();
  scene->computeBndles.clear();

  // Static mesh rendering
  {
    // Each mesh renderer has its own pipeline, binding table, vertex/index buffer(s) & draw type
    // (direct/indirect). So each mesh renderer should provide a command bundle.
    std::vector<rtMeshManager::renderInfo> infos;
    for (entityHnd_ecstapi ntt : scene->entities) {
      compType_ecstapi  compType = {};
      defaultComponent_rt* comp     = ( defaultComponent_rt* )editorECS->get_component_byEntityHnd(
        ntt, rtSceneManager_private::defaultCompTypeID, &compType);
      assert(comp && "Default component isn't found!");
      for (rtMesh mesh : comp->m_meshes) {
        rtMeshManager::renderInfo info;
        info.mesh      = mesh;
        info.transform = ( rtMat4* )&comp->m_worldTransform;
        infos.push_back(info);
      }
    }
    uint32_t               bndleCount   = 0;
    commandBundle_tgfxhnd* rasterBndles = rtMeshManager::renderMeshes(infos.size(), infos.data(), &bndleCount);
    scene->rasterBndles.insert(scene->rasterBndles.end(), rasterBndles, rasterBndles + bndleCount);
  }

  for (commandBundle_tgfxhnd bundle : scene->rasterBndles) {
    rtRenderer::rasterize(bundle);
  }
  for (commandBundle_tgfxhnd computeBundle : scene->computeBndles) {
    rtRenderer::compute(computeBundle);
  }
}