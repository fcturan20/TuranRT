#pragma once
#ifdef __cplusplus
extern "C" {
#endif
// Include editor_includes.h, ecs_tapi & resourceManager.h before this

struct rtMeshComponent {
  struct rtMesh**                 m_meshes;
  struct rtShaderEffectInstance** m_SEIs;
  unsigned int                    m_meshCount;
  struct rtMat4                       m_worldTransform;

  struct tapi_ecs_entity** m_children;
  unsigned int      m_childrenCount;
};

struct rtScene;
// rtSceneManager uses single-component entities
struct rtSceneManager {
  struct rtScene* (*createScene)();
  unsigned char (*destroyScene)(struct rtScene* scene);

  struct tapi_ecs_entity* (*addEntity)(struct rtScene* scene, struct tapi_ecs_entityType* type);

  struct tapi_ecs_componentTypeID* (*getMeshComponentTypeID)();
  struct tapi_ecs_entityType* (*getMeshEntityType)();
  const struct rtResourceManagerType* (*getResourceManagerType)();
};
extern const struct rtSceneManager* sceneManager;
void initializeSceneManager();


#ifdef __cplusplus
}
#endif