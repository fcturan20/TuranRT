#pragma once
#ifdef __cplusplus
extern "C" {
#endif
// Include ecs_tapi & resourceManager.h before this

struct defaultComponent_rt {
  std::vector<struct rtMesh*> m_meshes;
  glm::mat4           m_worldTransform;

  std::vector<entityHnd_ecstapi> m_children;
};

compTypeID_ecstapi getDefaultComponentTypeID();
entityHnd_ecstapi SM_addDefaultEntity(struct rtScene* scene);
void              SM_renderScene(struct rtScene* scene);

struct rtScene* SM_createScene();
bool            SM_destroyScene();

void                                SM_initializeManager();
const struct rtResourceManagerType* SM_managerType();

#ifdef __cplusplus
}
#endif