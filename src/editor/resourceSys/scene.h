#pragma once
// Include ecs_tapi & resourceManager.h before this

typedef struct scene_rt* rtScene;

typedef struct defaultComponent_rt {
  std::vector<rtMesh> m_meshes;
  glm::mat4           m_worldTransform;

  std::vector<entityHnd_ecstapi> m_children;
  static compTypeID_ecstapi      getDefaultComponentTypeID();
} rtDefaultComponent;

typedef struct sceneModifier_rt {
  static entityHnd_ecstapi addDefaultEntity(rtScene scene);
  static void renderScene(rtScene scene);
} rtSceneModifier;

typedef struct sceneManager_rt {
  static rtScene createScene();
  static bool    destroyScene();

  static void                  initializeManager();
  typedef rtScene              defaultResourceType;
  static rtResourceManagerType managerType();
} rtSceneManager;
