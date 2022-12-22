#pragma once
#include "assimp/types.h"
#include "ecs_tapi.h"
#include "resourceManager.h"

typedef struct scene_rt* rtScene;

struct aiNode;
typedef struct mesh_rt* rtMesh;
typedef struct sceneModifier_rt {
  static entityHnd_ecstapi addDefaultEntity(rtScene scene);
  // @param: Meshes should be in the same order as aiScene's
  static void createEntitiesWithAssimp(rtScene scene, aiNode* rootNode, rtMesh* const meshes);
  static void renderScene(rtScene scene);
} rtSceneModifier;

typedef struct sceneManager_rt {
  static rtScene createScene();
  static bool    destroyScene();

  static void                  initializeManager();
  typedef rtScene              defaultResourceType;
  static rtResourceManagerType managerType();
} rtSceneManager;
