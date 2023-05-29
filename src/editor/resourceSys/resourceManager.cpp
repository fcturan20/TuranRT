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

struct rtResource {
  std::string           filePath;
  bool                  isBinary;
  void*                 resourcePtr;
  const rtResourceManagerType* resourceManager;
  // Manager infos isn't used for now
};

struct rtResourceManager_private {
  static std::vector<rtResource*>            resources;
  static std::vector<rtResourceManagerType*> managers;
  static uint32_t                           maxResourceTypeInt;
  static const char*                        listDiskPath;
};
std::vector<rtResource*>            rtResourceManager_private::resources = {};
std::vector<rtResourceManagerType*> rtResourceManager_private::managers  = {};

struct rtResourceManagerType {
  std::string                               managerName;
  uint32_t                                  managerVer;
  RM_deserializeResourceFnc                 deserialize;
  RM_isResourceValidFnc                     validate;
};

struct rtResource* RM_createResource(const struct rtResourceDesc* desc) {
  assert(desc->pathNameExt && "Resource path name should be valid!");
  /*
  if (!desc.managerType->validate(desc.resourceHnd)) {
    printf("Resource should be valid!");
    return nullptr;
  }*/

  // It should first check already created resources, if there is such thing then return it.
  // With this way, rtResourceManager can automatically load resources at the startup and app can
  // get resource handles by describing them.

  rtResource* resource       = new rtResource;
  resource->filePath        = desc->pathNameExt;
  resource->isBinary        = desc->isBinary;
  resource->resourcePtr     = desc->resourceHnd;
  resource->resourceManager = desc->managerType;
  rtResourceManager_private::resources.push_back(resource);
  return resource;
}

const struct rtResourceManagerType* RM_registerManager(RM_managerDesc desc) {
  rtResourceManagerType* manager  = new rtResourceManagerType;
  manager->deserialize            = desc.deserialize;
  manager->validate               = desc.validate;
  manager->managerName            = desc.managerName;
  manager->managerVer             = desc.managerVer;
  return manager;
}

unsigned char RM_deserializeResource(const rtResourceDesc* desc, rtResource** resourceHnd) {
  for (rtResource* resource : rtResourceManager_private::resources) {
    if (desc->isBinary == resource->isBinary &&
        !strcmp(desc->pathNameExt, resource->filePath.data())) {
      *resourceHnd = resource;
    }
  }
  if (!resourceHnd) {
    *resourceHnd = RM_createResource(desc);
  }
  for (rtResourceManagerType* manager : rtResourceManager_private::managers) {
    if (manager->deserialize(desc)) {
      (*resourceHnd)->resourcePtr = desc->resourceHnd;
      return true;
    }
  }
  return false;
}
extern struct rtResource** importFileUsingAssimp(const wchar_t* i_PATH, uint64_t* resourceCount);
extern struct rtResource** importFileUsingGLTF(const wchar_t* i_PATH, uint64_t* resourceCount);
struct rtResource**        RM_importFile(const wchar_t* i_PATH, uint64_t* resourceCount) {
  if (!importFileUsingGLTF(i_PATH, resourceCount)) {
    return importFileUsingAssimp(i_PATH, resourceCount);
  }
}

void* RM_getResourceHnd(struct rtResource* resource, const struct rtResourceManagerType** managerType) {
  *managerType = resource->resourceManager;
  return resource->resourcePtr;
}