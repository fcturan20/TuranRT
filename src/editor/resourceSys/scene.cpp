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

static unsigned char                deserializeScene(const rtResourceDesc* desc) { return false; }
static unsigned char                isSceneValid(void* dataHnd) { return false; }
static const rtResourceManagerType* defmanagerType;
static entityTypeHnd_ecstapi        defaultEntityType;
static compTypeID_ecstapi           defaultCompTypeID;

const struct rtResourceManagerType* SM_managerType() { return defmanagerType; }

componentHnd_ecstapi createDefaultComp() { return (componentHnd_ecstapi) new defaultComponent_rt; }
void destroyDefaultComp(componentHnd_ecstapi comp) { delete ( defaultComponent_rt* )comp; }
void SM_initializeManager() {
  RM_managerDesc desc;
  desc.managerName                    = "Default Scene Resource Manager";
  desc.managerVer                     = MAKE_PLUGIN_VERSION_TAPI(0, 0, 0);
  desc.deserialize                    = deserializeScene;
  desc.validate                       = isSceneValid;
  defmanagerType                      = RM_registerManager(desc);

  componentManager_ecs compManager;
  compManager.createComponent  = createDefaultComp;
  compManager.destroyComponent = destroyDefaultComp;
  defaultCompTypeID =
    editorECS->addComponentType("Simple Component", nullptr, compManager, nullptr, 0);
  defaultEntityType =
    editorECS->addEntityType(&defaultCompTypeID, 1);
}

struct rtScene {
  std::vector<entityHnd_ecstapi>     entities;
  bindingTable_tgfxhnd               table;
  std::vector<commandBundle_tgfxhnd> rasterBndles;
  std::vector<commandBundle_tgfxhnd> computeBndles;
};

rtScene* SM_createScene() { return new rtScene; }
bool    SM_destroyScene() { return false; }

compType_ecstapi  compType = {};
entityHnd_ecstapi SM_addDefaultEntity(struct rtScene* scene) {
  entityHnd_ecstapi ntt = editorECS->createEntity(defaultEntityType);
  scene->entities.push_back(ntt);
  return ntt;
}

compTypeID_ecstapi getDefaultComponentTypeID() {
  return defaultCompTypeID;
}
void               SM_renderScene(struct rtScene* scene) {
  scene->rasterBndles.clear();
  scene->computeBndles.clear();

  // Static mesh rendering
  {
    // Each mesh renderer has its own pipeline, binding table, vertex/index buffer(s) & draw type
    // (direct/indirect). So each mesh renderer should provide a command bundle.
    std::vector<MM_renderInfo> infos;
    for (entityHnd_ecstapi ntt : scene->entities) {
      compType_ecstapi  compType = {};
      defaultComponent_rt* comp     = ( defaultComponent_rt* )editorECS->get_component_byEntityHnd(
        ntt, defaultCompTypeID, &compType);
      assert(comp && "Default component isn't found!");
      for (rtMesh* mesh : comp->m_meshes) {
        MM_renderInfo info;
        info.mesh      = mesh;
        info.transform = ( rtMat4* )&comp->m_worldTransform;
        infos.push_back(info);
      }
    }
    uint32_t               bndleCount   = 0;
    commandBundle_tgfxhnd* rasterBndles = MM_renderMeshes(infos.size(), infos.data(), &bndleCount);
    scene->rasterBndles.insert(scene->rasterBndles.end(), rasterBndles, rasterBndles + bndleCount);
  }

  for (commandBundle_tgfxhnd bundle : scene->rasterBndles) {
    rtRenderer::rasterize(bundle);
  }
  for (commandBundle_tgfxhnd computeBundle : scene->computeBndles) {
    rtRenderer::compute(computeBundle);
  }
}