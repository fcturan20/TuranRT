#pragma once
#ifdef __cplusplus
extern "C" {
#endif

// This renderer is to render 3D world scenes
// Each rendering instance can be used to render different scenes

struct teSceneRenderer {
  struct teSceneRenderingInstance* (*createRenderingInstance)(const struct rtScene* scene);
  void (*changeMainView)(struct teSceneRenderingInstance* renderer, struct teCamera* cam);
};
extern const struct teSceneRenderer* sceneRenderer;

void initializeSceneRenderer();

#ifdef __cplusplus
}
#endif