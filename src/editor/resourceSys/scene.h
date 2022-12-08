#pragma once
#include "assimp/types.h"
#include "ecs_tapi.h"
#include "resourceManager.h"

typedef struct scene_rt* rtScene;

struct aiNode;
typedef struct sceneModifier_rt {
  static entityHnd_ecstapi addDefaultEntity(rtScene scene);
  static void              createEntitiesWithAssimp(aiNode* rootNode);
} rtSceneModifier;

typedef struct sceneManager_rt {
  static rtScene createScene();
  static bool    destroyScene();

  static void                  initializeManager();
  typedef rtScene              defaultResourceType;
  static rtResourceManagerType managerType();
} rtSceneManager;
