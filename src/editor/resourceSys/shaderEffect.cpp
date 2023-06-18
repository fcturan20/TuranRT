#include <vector>

#include <predefinitions_tapi.h>
#include <virtualmemorysys_tapi.h>
#include <allocator_tapi.h>
#include <tgfx_structs.h>

#include "../editor_includes.h"
#include "resourceManager.h"
#include "shaderEffect.h"

static constexpr const char* pushConstantCode =
  "layout( push_constant ) uniform constants{uint queueIndx, passTypeIndx, passInstanceIndx, "
  "commandBundleIndx, pipelineTypeIndx, callIndx; } PushConstants;\n"
  "uint get_callID() { return PushConstants.callIndx;} uint get_pipelineID(){return "
  "PushConstants.pipelineTypeIndx;} uint get_commandBundleID(){return "
  "PushConstants.commandBundleIndx;} uint get_passInstanceID(){return "
  "PushConstants.passInstanceIndx;} uint get_passTypeID(){return PushConstants.passTypeIndx;} "
  "uint get_queueID(){return PushConstants.queueIndx;}";
static constexpr uint32_t pushConstantOffset = 0, pushConstantSize = 24;
/*
static unsigned char deserializeSE(const struct rtResourceDesc* desc) { return false; }
static unsigned char isSEValid(void* dataHnd) { return false; }
std::vector<rtShaderEffectManagerType*> SEmts;
struct rtShaderEffectManagerType {
  SEM_managerDesc desc;
};
struct rtShaderEffectBase {
  const rtShaderEffectManagerType* SEMT;
};
struct rtShaderEffectInstance {
  rtShaderEffect* effect;
};
const rtResourceManagerType* ShaderEffectRMT = {};

const struct rtResourceManagerType* SEM_managerType() { return ShaderEffectRMT; }

void SEM_getBindingTableDesc(struct rtShaderEffect*                    shader,
                             const struct rtShaderEffectInstanceInput* instanceInput,
                             struct tgfx_bindingTableDescription*      desc) {

}

struct rtShaderEffect* SEM_getSE(const struct rtShaderEffectInstance* instance) { return nullptr; }
void* SEM_createShaderEffectHandle(const struct rtShaderEffectManagerType* managerType) {
  rtShaderEffectBase* base = ( rtShaderEffectBase* )malloc(
    sizeof(rtShaderEffectBase) + managerType->desc.shaderEffectStructSize);
  base->SEMT = managerType;
  return ( void* )(uintptr_t(base) + sizeof(rtShaderEffectBase));
}

const struct rtShaderEffectManagerType* SEM_registerManager(SEM_managerDesc desc) {
  rtShaderEffectManagerType* SEmt = new rtShaderEffectManagerType;
  SEmt->desc                      = desc;
  
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
}*/