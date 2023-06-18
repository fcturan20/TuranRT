#include <stdint.h>
#include <tgfx_forwarddeclarations.h>

#include <glm/glm.hpp>

#include "../editor_includes.h"
#include "rendercontext.h"
#include "forwardRenderer.h"
#include "../resourceSys/scene.h"
#include "../systems/camera.h"
#include "sceneRenderer.h"

struct rtGpuMemBlock* m_gpuCamBuffer;
uint32_t              m_gpuCameraBufferBindingID = UINT32_MAX;
struct teGpuCamera {
  glm::mat4 worldToView;
  glm::mat4 viewToProj;
  glm::mat4 viewToWorld;
  glm::vec4 pos_fov;
  glm::vec4 dir;
};

void createBuffersAndTextures() {
  // Create camera upload buffer and bind it to camera binding table
  {
    m_gpuCamBuffer =
      allocateBuffer(sizeof(teGpuCamera) * swapchainTextureCount,
                     bufferUsageMask_tgfx_COPYTO | bufferUsageMask_tgfx_STORAGEBUFFER,
                     getGpuMemRegion(rtMemoryRegionType_UPLOAD));
    bindViewBuffer();
  }

#ifdef NDEBUG
  logSys->log(log_type_tapi_STATUS, false, L"createDeviceLocalResources() finished");
#endif
}
void bindViewBuffer() {
  // Bind view buffer
  for (uint32_t frameIndx = 0; frameIndx < swapchainTextureCount; frameIndx++) {
    uint32_t m_bufferOffset = 0, m_bufferSize = m_gpuCamBuffer->size;
    renderer->bindBuffer(m_gpuCameraBufferBindingID, m_gpuCamBuffer, 0, sizeof());
  }
}
struct teSceneRenderingInstance {
  struct teRendererInstance*       rendererInstanceHandle;
  const struct rtScene*                  scene;
  static teSceneRenderingInstance* createInstance(const struct rtScene* scene) {
    teSceneRenderingInstance* renderingInstance = new teSceneRenderingInstance;
    rtRendererDescription     desc;
    desc.data                                 = renderingInstance;
    desc.renderFrame                          = teSceneRenderingInstance::renderFrame;
    desc.close                                = teSceneRenderingInstance::close;
    renderingInstance->rendererInstanceHandle = renderer->registerRendererInstance(&desc);
    renderingInstance->scene                  = scene;
  }
  static void changeMainView(struct teSceneRenderingInstance* instance, struct teCamera* cam) {}
  static void renderFrame(void* data) {
    teSceneRenderingInstance* instance = ( teSceneRenderingInstance* )data;
  }
  static void close(void* data) {
    teSceneRenderingInstance* instance = ( teSceneRenderingInstance* )data;
  }
};
void SM_renderScene(struct rtScene* scene) {
  for (uint32_t cameraIndx = 0; cameraIndx < scene->allCameras; cameraIndx++) {
    upload(cameraController->getCameraMatrixes(true, , , , , ));
  }

  // Static mesh rendering
  {
    // Each mesh renderer has its own pipeline, binding table, vertex/index buffer(s) & draw type
    // (direct/indirect). So each mesh renderer should provide a command bundle.
    std::vector<MM_renderInfo> infos;
    for (tapi_ecs_entity* ntt : scene->entities) {
      void*            compType = {};
      rtMeshComponent* comp =
        ( rtMeshComponent* )editorECS->get_component_byEntityHnd(ntt, defaultCompTypeID, &compType);
      assert(comp && "Default component isn't found!");
      for (uint32_t i = 0; i < comp->m_meshCount; i++) {
        MM_renderInfo info;
        info.mesh      = comp->m_meshes[i];
        info.transform = ( rtMat4* )&comp->m_worldTransform;
        info.sei       = comp->m_SEIs[i];
        infos.push_back(info);
      }
    }
    uint32_t                    bndleCount = 0;
    struct tgfx_commandBundle** rasterBndles =
      MM_renderMeshes(infos.size(), infos.data(), &bndleCount);
    scene->rasterBndles.insert(scene->rasterBndles.end(), rasterBndles, rasterBndles + bndleCount);
  }


  // Record & queue upload command buffers
  {
    struct tgfx_commandBuffer* uploadCmdBuffer = tgfxRenderer->beginCommandBuffer(queue, 0, {});
    tgfxRenderer->executeBundles(uploadCmdBuffer, m_uploadBundles.size(), m_uploadBundles.data(), 0,
                                 {});
    tgfxRenderer->endCommandBuffer(uploadCmdBuffer);
    tgfxRenderer->queueExecuteCmdBuffers(queue, 1, &uploadCmdBuffer, 0, nullptr);
  }
  // Record frame's raster & compute command buffers, then queue it
  {
    // Raster
    struct tgfx_commandBuffer* rasterCB = tgfxRenderer->beginCommandBuffer(queue, 0, nullptr);
    colorAttachmentInfo.texture         = m_swapchainTextures[m_activeSwpchnIndx];
    tgfxRenderer->beginRasterpass(rasterCB, 1, &colorAttachmentInfo, &depthAttachmentInfo, 0,
                                  nullptr);
    tgfxRenderer->executeBundles(rasterCB, m_rasterBndls.size(), m_rasterBndls.data(), 0, nullptr);
    tgfxRenderer->endRasterpass(rasterCB, 0, nullptr);
    tgfxRenderer->endCommandBuffer(rasterCB);
    tgfxRenderer->queueExecuteCmdBuffers(queue, 1, &rasterCB, 0, nullptr);

    // Compute
    struct tgfx_commandBuffer* computeCB = tgfxRenderer->beginCommandBuffer(queue, 0, nullptr);
    tgfxRenderer->executeBundles(computeCB, m_computeBndls.size(), m_computeBndls.data(), 0,
                                 nullptr);
    tgfxRenderer->endCommandBuffer(computeCB);
    tgfxRenderer->queueExecuteCmdBuffers(queue, 1, &computeCB, 0, nullptr);
  }
}

void initializeSceneRenderer() {
  // Create system struct
  teSceneRenderer* r         = new teSceneRenderer;
  r->createRenderingInstance = teSceneRenderingInstance::createInstance;
  r->changeMainView          = teSceneRenderingInstance::changeMainView;
  sceneRenderer              = r;

  m_gpuCameraBufferBindingID = renderer->allocateBinding(shaderdescriptortype_tgfx_BUFFER);
}




/*
objList       mainWorldObjects;
spotLightList spotLights;

void worldRenderer::prepare() {
  teForwardPassDescription mainViewDesc;
  mainViewDesc.viewportInfo    = getViewportInfo();
  tePassInstance* mainViewPass = teForwardPass->createPassInstance(&mainViewDesc);
  mainViewPass->prepare(mainWorldObjects);

  vector<tePassInstance*> spotLightPasses(spotLights.size());
  for (spotLight* light : spotLights) {
    teShadowPassDescription lightViewDesc;
    lightViewDesc.res         = 1 / length(mainViewDesc.viewportInfo.cam.pos - light->pos);
    lightViewDesc.camera      = light->cam;
    tePassInstance* lightPass = teShadowRenderer->createPassInstance();
    lightPass->prepare(mainWorldObjects);
    spotLightPasses[i] = lightPass;
  }

  tgfx_commandBuffer* worldRenderingCB = tgfx_renderer->beginCB();
}*/