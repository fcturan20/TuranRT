#include <vector>

#include <predefinitions_tapi.h>
#include <tgfx_forwarddeclarations.h>

#include "../editor_includes.h"
#include "resourceManager.h"
#include "shaderEffect.h"
#include "surfaceSE.h"
#include "mesh.h"

static constexpr const char* surfaceShaderEffectManagerName = "Surface ShaderEffect Manager Type";
rtShaderEffectManagerType    surfaceSEmt                    = {};

rtShaderEffectManagerType surfaceShaderEffectManager_rt::managerType() { return surfaceSEmt; }

struct surfaceShaderEffect_rt {
  rtDescription_surfaceSE                 desc;
  std::vector<rtShaderEffectInstanceInfo> instanceInformations;
};
rtShaderEffect createSurfaceShaderEffect(void* extraInfo) {
  auto surfaceSFX =
    ( surfaceShaderEffect_rt* )shaderEffectManager_rt::createShaderEffectHandle(surfaceSEmt);
  surfaceSFX->desc = *( rtDescription_surfaceSE* )extraInfo;

  return ( rtShaderEffect )surfaceSFX;
}
unsigned int getSurfaceShaderEffectInstanceInfo(rtShaderEffect              sfx,
                                                rtShaderEffectInstanceInfo* infos) {
  auto effect = ( surfaceShaderEffect_rt* )sfx;
  if (infos) {
    memcpy(infos, effect->instanceInformations.data(),
           sizeof(rtShaderEffectInstanceInfo) * effect->instanceInformations.size());
  }
  return effect->instanceInformations.size();
}
unsigned char destroySurfaceShaderEffect(rtShaderEffect shaderEffect) { return 0; }
void          frameSurfaceShaderEffect() {}

void surfaceShaderEffectManager_rt::initializeManager() {
  rtShaderEffectManager::managerDesc desc;
  desc.managerName                   = surfaceShaderEffectManagerName;
  desc.managerVer                    = MAKE_PLUGIN_VERSION_TAPI(0, 0, 0);
  desc.shaderEffectStructSize        = sizeof(surfaceShaderEffect_rt);
  desc.createShaderEffectFnc         = createSurfaceShaderEffect;
  desc.destroyShaderEffectFnc        = destroySurfaceShaderEffect;
  desc.getRtShaderEffectInstanceInfo = getSurfaceShaderEffectInstanceInfo;
  desc.frameFnc                      = frameSurfaceShaderEffect;
  surfaceSEmt                        = rtShaderEffectManager::registerManager(desc);
}