#include <assert.h>
#include <vector>
#include <string>
#include <glm/glm.hpp>

#include <logger_tapi.h>
#include <string_tapi.h>
#include <ecs_tapi.h>
#include <tgfx_forwarddeclarations.h>

#include "../editor_includes.h"
#include "resourceManager.h"
#include "mesh.h"
#include "scene.h"

struct resource_rt {
  std::string           filePath;
  bool                  isBinary;
  void*                 resourcePtr;
  rtResourceManagerType resourceManager;
  // Manager infos isn't used for now
};

struct rtResourceManager_private {
  static std::vector<rtResource>            resources;
  static std::vector<rtResourceManagerType> managers;
  static uint32_t                           maxResourceTypeInt;
  static const char*                        listDiskPath;
};
std::vector<rtResource>            rtResourceManager_private::resources = {};
std::vector<rtResourceManagerType> rtResourceManager_private::managers  = {};

struct resourceManagerType_rt {
  std::string                               managerName;
  uint32_t                                  managerVer;
  rtResourceManager::deserializeResourceFnc deserialize;
  rtResourceManager::isResourceValidFnc     validate;
};

rtResource resourceManager_rt::createResource(rtResourceDesc desc) {
  assert(desc.pathNameExt && "Resource path name should be valid!");
  /*
  if (!desc.managerType->validate(desc.resourceHnd)) {
    printf("Resource should be valid!");
    return nullptr;
  }*/

  // It should first check already created resources, if there is such thing then return it.
  // With this way, rtResourceManager can automatically load resources at the startup and app can
  // get resource handles by describing them.

  rtResource resource       = new resource_rt;
  resource->filePath        = desc.pathNameExt;
  resource->isBinary        = desc.isBinary;
  resource->resourcePtr     = desc.resourceHnd;
  resource->resourceManager = desc.managerType;
  rtResourceManager_private::resources.push_back(resource);
  return resource;
}

rtResourceManagerType resourceManager_rt::registerManager(managerDesc desc) {
  resourceManagerType_rt* manager = new resourceManagerType_rt;
  manager->deserialize            = desc.deserialize;
  manager->validate               = desc.validate;
  manager->managerName            = desc.managerName;
  manager->managerVer             = desc.managerVer;
  return manager;
}

bool resourceManager_rt::deserializeResource(rtResourceDesc* desc, rtResource* resourceHnd) {
  for (rtResource resource : rtResourceManager_private::resources) {
    if (desc->isBinary == resource->isBinary &&
        !strcmp(desc->pathNameExt, resource->filePath.data())) {
      *resourceHnd = resource;
    }
  }
  if (!resourceHnd) {
    *resourceHnd = createResource(*desc);
  }
  for (rtResourceManagerType manager : rtResourceManager_private::managers) {
    if (manager->deserialize(desc)) {
      (*resourceHnd)->resourcePtr = desc->resourceHnd;
      return true;
    }
  }
  return false;
}
extern rtResource* importFileUsingAssimp(const wchar_t* i_PATH, uint64_t* resourceCount);
extern rtResource* importFileUsingGLTF(const wchar_t* i_PATH, uint64_t* resourceCount);
rtResource* resourceManager_rt::importFile(const wchar_t* i_PATH, uint64_t* resourceCount) {
  if (!importFileUsingGLTF(i_PATH, resourceCount)) {
    return importFileUsingAssimp(i_PATH, resourceCount);
  }
}

void* resourceManager_rt::getResourceHnd(rtResource resource, rtResourceManagerType& managerType) {
  managerType = resource->resourceManager;
  return resource->resourcePtr;
}