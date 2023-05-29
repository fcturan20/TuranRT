#include <vector>

#include <predefinitions_tapi.h>
#include <tgfx_structs.h>

#include "resourceManager.h"
#include "shaderEffect.h"

static bool                            deserializeSE(rtResourceDesc* desc) { return false; }
static bool                            isSEValid(void* dataHnd) { return false; }
std::vector<rtShaderEffectManagerType> SEmts;
struct shaderEffectManagerType_rt {
  SEM_managerDesc desc;
};
rtResourceManagerType ShaderEffectRMT = {};

const struct rtResourceManagerType* SEM_managerType() { return ShaderEffectRMT; }

void                   SEM_getBindingTableDesc(struct rtShaderEffect*                    shader,
                                               const struct rtShaderEffectInstanceInput* instanceInput,
                                               struct tgfx_binding_table_description*    desc) {}
struct rtShaderEffect* SEM_getSE(const struct rtShaderEffectInstance* instance) { return nullptr; }

const struct rtShaderEffectManagerType* SEM_registerManager(SEM_managerDesc desc) {
  shaderEffectManagerType_rt* SEmt = new shaderEffectManagerType_rt;
  SEmt->desc                       = desc;
  SEmts.push_back(SEmt);
  return SEmt;
}
void SEM_initializeManager() {
  RM_managerDesc desc;
  desc.managerName = "ShaderEffect Resource Manager";
  desc.managerVer  = MAKE_PLUGIN_VERSION_TAPI(0, 0, 0);
  desc.deserialize = deserializeSE;
  desc.validate    = isSEValid;
  ShaderEffectRMT  = RM_registerManager(desc);
}