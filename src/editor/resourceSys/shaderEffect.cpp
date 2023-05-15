#include <vector>

#include <predefinitions_tapi.h>

#include "resourceManager.h"
#include "shaderEffect.h"

static bool                            deserializeSE(rtResourceDesc* desc) { return false; }
static bool                            isSEValid(void* dataHnd) { return false; }
std::vector<rtShaderEffectManagerType> SEmts;
struct shaderEffectManagerType_rt {
  rtShaderEffectManager::managerDesc desc;
};
rtResourceManagerType ShaderEffectRMT = {};

rtResourceManagerType shaderEffectManager_rt::managerType() { return ShaderEffectRMT; }

rtShaderEffectManagerType shaderEffectManager_rt::registerManager(managerDesc desc) {
  shaderEffectManagerType_rt* SEmt = new shaderEffectManagerType_rt;
  SEmt->desc                      = desc;
  SEmts.push_back(SEmt);
  return Mmt;
}
void shaderEffectManager_rt::initializeManager() {
  rtResourceManager::managerDesc desc;
  desc.managerName = "ShaderEffect Resource Manager";
  desc.managerVer  = MAKE_PLUGIN_VERSION_TAPI(0, 0, 0);
  desc.deserialize = deserializeSE;
  desc.validate    = isSEValid;
  ShaderEffectRMT  = rtResourceManager::registerManager(desc);
}