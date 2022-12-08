#include <assert.h>
#include <vector>
// Assimp libraries to load Model
#include <assimp/postprocess.h>
#include <assimp/scene.h>
#include <assimp/Importer.hpp>
#include <glm/glm.hpp>

#include "resourceManager.h"
#include "mesh.h"
#include "scene.h"

struct resource_rt {
  std::string filePath;
  bool        isBinary;
  uint32_t    resourceType;
  void*       resourcePtr;
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

rtResource rtResourceManager::createResource(rtResourceDesc desc) {
  assert(desc.pathNameExt && "Resource path name should be valid!");
  /*
  if (!desc.managerType->validate(desc.resourceHnd)) {
    printf("Resource should be valid!");
    return nullptr;
  }*/

  // It should first check already created resources, if there is such thing then return it.
  // With this way, rtResourceManager can automatically load resources at the startup and app can
  // get resource handles by describing them.

  rtResource resource   = new resource_rt;
  resource->filePath    = desc.pathNameExt;
  resource->isBinary    = desc.isBinary;
  resource->resourcePtr = desc.resourceHnd;
  rtResourceManager_private::resources.push_back(resource);
  return resource;
}

rtResourceManagerType rtResourceManager::registerManager(managerDesc desc) {
  resourceManagerType_rt* manager = new resourceManagerType_rt;
  manager->deserialize            = desc.deserialize;
  manager->validate               = desc.validate;
  manager->managerName            = desc.managerName;
  manager->managerVer             = desc.managerVer;
  return manager;
}

bool rtResourceManager::deserializeResource(rtResourceDesc* desc, rtResource* resourceHnd) {
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

rtResource* rtResourceManager::importAssimp(const char* PATH, uint64_t* resourceCount) {
  Assimp::Importer import;
  const aiScene*   aScene = nullptr;
  {
    aScene = import.ReadFile(PATH, 0 | aiProcess_GenNormals | aiProcess_CalcTangentSpace |
                                     aiProcess_FindDegenerates | aiProcess_FlipUVs |
                                     aiProcess_Triangulate | aiProcess_GenUVCoords |
                                     aiProcess_FindInvalidData);

    // Check if scene reading errors!
    if (!aScene || aScene->mFlags & AI_SCENE_FLAGS_INCOMPLETE || !aScene->mRootNode) {
      printf("FAIL: Assimp couldn't load the file. Log: %s\n", import.GetErrorString());
      return nullptr;
    }
  }

  std::vector<rtResource> resources;
  for (uint32_t i = 0; i < aScene->mNumMeshes; i++) {
    rtMesh mesh = rtMeshManager::createDefaultMesh(aScene->mMeshes[i]);
    if (!mesh) {
      continue;
    }

    rtResourceDesc resourceDesc = {};
    resourceDesc.managerType    = rtMeshManager::managerType();
    resourceDesc.resourceHnd    = mesh;
    std::string meshName        = SOURCE_DIR "Content/Meshes/";

    bool isThereNameCollision = false;
    // TODO: Check against name collision across meshes in the same Assimp Scene.

    if (aScene->mMeshes[i]->mName.length && !isThereNameCollision) {
      meshName += aScene->mMeshes[i]->mName.C_Str();
    } else {
      // TODO: Find the first aiNode in the scene that references this mesh
      //  then use entity's name as mesh name
    }

    resourceDesc.pathNameExt = meshName.c_str();
    resources.push_back(rtResourceManager::createResource(resourceDesc));
  }

  {
    rtScene scene = rtSceneManager::createScene();
    rtSceneModifier::createEntitiesWithAssimp(aScene->mRootNode);
    rtResourceDesc resourceDesc = {};
    resourceDesc.managerType    = rtSceneManager::managerType();
    resourceDesc.resourceHnd    = scene;
    std::string meshName        = SOURCE_DIR "Content/Scenes/";

    if (aScene->mRootNode->mName.length) {
      meshName += aScene->mRootNode->mName.C_Str();
    } else {
      meshName += aScene->GetShortFilename(PATH);
    }
    resourceDesc.pathNameExt = meshName.c_str();

    resources.push_back(rtResourceManager::createResource(resourceDesc));
  }

  new rtResource[resources.size()];
  return nullptr;
}