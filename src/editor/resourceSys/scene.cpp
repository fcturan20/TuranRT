#include <assimp/postprocess.h>
#include <assimp/scene.h>
#include <assimp/Importer.hpp>
#include <glm/glm.hpp>
#include <vector>

#include <tgfx_forwarddeclarations.h>
#include <tgfx_renderer.h>

#include "../editor_includes.h"
#include "ecs_tapi.h"
#include "resourceManager.h"
#include "scene.h"
#include "mesh.h"
#include "../render_context/rendercontext.h"

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

rtResourceManagerType rtSceneManager::managerType() { return rtSceneManager_private::managerType; }

struct defaultComponent {
  std::vector<rtMesh> m_meshes;
  glm::mat4           m_worldTransform;

  std::vector<entityHnd_ecstapi> m_children;
};
componentHnd_ecstapi createDefaultComp() { return (componentHnd_ecstapi) new defaultComponent; }
void destroyDefaultComp(componentHnd_ecstapi comp) { delete ( defaultComponent* )comp; }
void rtSceneManager::initializeManager() {
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

rtScene rtSceneManager::createScene() { return new scene_rt; }
bool    rtSceneManager::destroyScene() { return false; }

void fillEntity(rtScene scene, entityHnd_ecstapi ntt, aiNode* node, rtMesh* const meshes) {
  compType_ecstapi  compType = {};
  defaultComponent* comp     = ( defaultComponent* )editorECS->get_component_byEntityHnd(
        ntt, rtSceneManager_private::defaultCompTypeID, &compType);
  assert(comp && "Default component isn't found!");
  comp->m_meshes.resize(node->mNumMeshes);
  for (uint32_t i = 0; i < node->mNumMeshes; i++) {
    comp->m_meshes[i] = meshes[node->mMeshes[i]];
  }
  for (uint8_t i = 0; i < 4; i++) {
    for (uint8_t j = 0; j < 4; j++) {
      comp->m_worldTransform[i][j] = node->mTransformation[i][j];
    }
  }
  comp->m_children.resize(node->mNumChildren);
  for (uint32_t i = 0; i < node->mNumChildren; i++) {
    comp->m_children[i] = rtSceneModifier::addDefaultEntity(scene);
    fillEntity(scene, comp->m_children[i], node->mChildren[i], meshes);
  }
}
void rtSceneModifier::createEntitiesWithAssimp(rtScene scene, aiNode* rootNode,
                                               rtMesh* const meshes) {
  entityHnd_ecstapi rootNtt = addDefaultEntity(scene);
  fillEntity(scene, rootNtt, rootNode, meshes);
}

compType_ecstapi  compType = {};
entityHnd_ecstapi rtSceneModifier::addDefaultEntity(rtScene scene) {
  entityHnd_ecstapi ntt = editorECS->createEntity(rtSceneManager_private::defaultEntityType);
  scene->entities.push_back(ntt);
  return ntt;
}

void rtSceneModifier::renderScene(rtScene scene) {
  scene->rasterBndles.clear();
  scene->computeBndles.clear();

  // Static mesh rendering
  {
    // Each mesh renderer has its own pipeline, binding table, vertex/index buffer(s) & draw type
    // (direct/indirect). So each mesh renderer should provide a command bundle.
    std::vector<meshManager_rt::renderInfo> infos;
    for (entityHnd_ecstapi ntt : scene->entities) {
      compType_ecstapi  compType = {};
      defaultComponent* comp     = ( defaultComponent* )editorECS->get_component_byEntityHnd(
        ntt, rtSceneManager_private::defaultCompTypeID, &compType);
      assert(comp && "Default component isn't found!");
      for (rtMesh mesh : comp->m_meshes) {
        meshManager_rt::renderInfo info;
        info.mesh      = mesh;
        info.transform = &comp->m_worldTransform;
        infos.push_back(info);
      }
    }
    commandBundle_tgfxhnd rasterBndl, computeBndl;
    meshManager_rt::render(infos.size(), infos.data(), &rasterBndl, &computeBndl);
    scene->rasterBndles.push_back(rasterBndl);
    scene->computeBndles.push_back(computeBndl);
  }

  for (commandBundle_tgfxhnd bundle : scene->rasterBndles) {
    rtRenderer::rasterize(bundle);
  }
  for (commandBundle_tgfxhnd computeBundle : scene->computeBndles) {
    rtRenderer::compute(computeBundle);
  }
}