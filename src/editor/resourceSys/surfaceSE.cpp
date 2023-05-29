#include <vector>

#include <predefinitions_tapi.h>
#include <tgfx_forwarddeclarations.h>
#include <tgfx_structs.h>

#include "../editor_includes.h"
#include "resourceManager.h"
#include "shaderEffect.h"
#include "surfaceSE.h"
#include "mesh.h"

static constexpr const char* surfaceShaderEffectManagerName = "Surface ShaderEffect Manager Type";
rtShaderEffectManagerType    surfaceSEmt                    = {};

rtShaderEffectManagerType SSEM_managerType() { return surfaceSEmt; }

struct rtSurfaceShaderEffect {
  rtDescription_surfaceSE                         desc;
  std::vector<struct rtShaderEffectInstanceInfo*> instanceInformations;
  struct pipelineMMTCouple {
    rtMeshManagerType mmt;
    pipeline_tgfxhnd  pipe;
  };
  std::vector<pipelineMMTCouple> pipelines;
};

////////////////////////////////// SURFACE SEM FUNCS

void SSEM_addPipeline(struct rtSurfaceShaderEffect* surfaceSfx, pipeline_tgfxhnd pipeline,
                      const struct rtMeshManagerType* meshManagerType) {
  rtSurfaceShaderEffect::pipelineMMTCouple couple;
  couple.mmt  = meshManagerType;
  couple.pipe = pipeline;
  surfaceSfx->pipelines.push_back(couple);
}

pipeline_tgfxhnd SSEM_getPipeline(struct rtSurfaceShaderEffect*   surfaceSfx,
                                  const struct rtMeshManagerType* meshManagerType) {
  for (uint32_t i = 0; i < surfaceSfx->pipelines.size(); i++) {
    if (surfaceSfx->pipelines[i].mmt == meshManagerType) {
      return surfaceSfx->pipelines[i].pipe;
    }
  }
  return nullptr;
}

////////////////////////////////// GENERAL SEM FUNCS

rtShaderEffect* createSurfaceShaderEffect(void* extraInfo) {
  rtDescription_surfaceSE* desc = ( rtDescription_surfaceSE* )extraInfo;
  return SSEM_createEffect(desc);
}
unsigned int getSurfaceShaderEffectInstanceInfo(rtShaderEffect*                     sfx,
                                                struct rtShaderEffectInstanceInfo** infos) {
  auto effect = ( rtSurfaceShaderEffect* )sfx;
  if (infos) {
    memcpy(infos, effect->instanceInformations.data(),
           sizeof(struct rtShaderEffectInstanceInfo*) * effect->instanceInformations.size());
  }
  return effect->instanceInformations.size();
}
unsigned char destroySurfaceShaderEffect(rtShaderEffect* shaderEffect) { return 0; }

void frameSurfaceShaderEffect() {}

struct rtSurfaceShaderEffect* SSEM_createEffect(const rtDescription_surfaceSE* desc) {
  if (!desc->shading_code || desc->language != shaderlanguages_tgfx_GLSL) {
    return nullptr;
  }

  auto surfaceSFX =
    ( surfaceShaderEffect_rt* )shaderEffectManager_rt::createShaderEffectHandle(surfaceSEmt);
  surfaceSFX->desc = *( rtDescription_surfaceSE* )extraInfo;

  return ( rtShaderEffect* )surfaceSFX;
}

rtDescription_surfaceSE SSEM_getSurfaceSEProps(struct rtSurfaceShaderEffect* surfaceSfx) {
  return surfaceSfx->desc;
}

void SSEM_initializeManager() {
  SEM_managerDesc desc;
  desc.managerName                      = surfaceShaderEffectManagerName;
  desc.managerVer                       = MAKE_PLUGIN_VERSION_TAPI(0, 0, 0);
  desc.shaderEffectStructSize           = sizeof(surfaceShaderEffect_rt);
  desc.createShaderEffectFnc            = createSurfaceShaderEffect;
  desc.destroyShaderEffectFnc           = destroySurfaceShaderEffect;
  desc.getRtShaderEffectInstanceInfoFnc = getSurfaceShaderEffectInstanceInfo;
  desc.frameFnc                         = frameSurfaceShaderEffect;
  surfaceSEmt                           = SEM_registerManager(desc);
}