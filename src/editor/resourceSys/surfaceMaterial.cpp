#include <vector>

#include <predefinitions_tapi.h>
#include <tgfx_forwarddeclarations.h>
#include <tgfx_structs.h>

#include "../editor_includes.h"
#include "resourceManager.h"
#include "shaderEffect.h"
#include "surfaceMaterial.h"
#include "mesh.h"

static constexpr const char*        surfaceMaterialManagerName = "Surface Material Manager Type";
const struct rtResourceManagerType* surfaceMaterialManagerType;

struct rtSurfaceMaterial {
  rtSurfaceMaterialDescription                    desc;
  std::vector<struct rtShaderEffectInstanceInfo*> instanceInformations;
};

////////////////////////////////// SURFACE SEM FUNCS

void surfaceRenderer_registerRenderer(const struct rtSurfaceRendererDescription* desc) {}
struct rtSurfaceMaterial*     SMM_createMaterial(const struct rtSurfaceMaterialDescription* desc) {}
struct rtPhongMaterialBuffer* SMM_createPhongMaterialInstance() {}

////////////////////////////////// GENERAL SEM FUNCS

struct rtShaderEffectInstance* SSEM_createPhongSEI() { return nullptr; }

struct rtShaderEffectInstance* SSEM_createUnlitSEI() { return nullptr; }

struct rtShaderEffectInstanceInput* SSEM_getDiffuseInput() { return nullptr; }

struct rtSurfaceShaderEffect* SSEM_createEffect(const rtDescription_surfaceSE* desc) {
  if (!desc->shading_code || desc->language != shaderlanguages_tgfx_GLSL) {
    return nullptr;
  }

  auto surfaceSFX  = ( rtSurfaceShaderEffect* )SEM_createShaderEffectHandle(surfaceSEmt);
  surfaceSFX->desc = *desc;

  return ( rtSurfaceShaderEffect* )surfaceSFX;
}

rtDescription_surfaceSE SSEM_getSurfaceSEProps(struct rtSurfaceShaderEffect* surfaceSfx) {
  return surfaceSfx->desc;
}

char_sc                        attribNames[]            = {"POSITION", "TEXCOORD0", "NORMAL"};
uint32_sc                      attribCount              = length_c(attribNames);
uint32_sc                      attribElementSize[]      = {3 * 4, 2 * 4, 3 * 4};
static constexpr datatype_tgfx attribElementTypesTgfx[] = {
  datatype_tgfx_VAR_VEC3, datatype_tgfx_VAR_VEC2, datatype_tgfx_VAR_VEC3};
static constexpr vertexBindingInputRate_tgfx attribInputRate = vertexBindingInputRate_tgfx_VERTEX;
static rtSurfaceShaderEffect *               defaultPhongLit = nullptr, *defaultUnlit = nullptr;
void                                         SSEM_initializeManager() {
  SEM_managerDesc desc;
  desc.managerName = surfaceShaderEffectManagerName;
  desc.managerVer  = MAKE_PLUGIN_VERSION_TAPI(0, 0, 0);
  desc.shaderEffectStructSize = sizeof(rtSurfaceShaderEffect);
  desc.createShaderEffectFnc  = createSurfaceShaderEffect;
  desc.destroyShaderEffectFnc = destroySurfaceShaderEffect;
  desc.getRtShaderEffectInstanceInfoFnc = getSurfaceShaderEffectInstanceInfo;
  desc.frameFnc = frameSurfaceShaderEffect;
  surfaceSEmt   = SEM_registerManager(desc);

  tgfx_samplerDescription samplerDesc = {};
  samplerDesc.magFilter = texture_mipmapfilter_tgfx_LINEAR_FROM_1MIP;
  samplerDesc.minFilter = texture_mipmapfilter_tgfx_LINEAR_FROM_1MIP;
  samplerDesc.maxMipLevel = 0;
  samplerDesc.minMipLevel = 0;
  samplerDesc.wrapDepth   = texture_wrapping_tgfx_REPEAT;
  samplerDesc.wrapHeight  = texture_wrapping_tgfx_REPEAT;
  samplerDesc.wrapWidth   = texture_wrapping_tgfx_REPEAT;
  rtShaderEffectInstanceInput* difInputs[2] = {
    SEM_createSEIInput_texture("DIFFUSETEXTURE", true, texture_channels_tgfx_RGBA8UB),
    SEM_createSEIInput_sampler("DIFFUSESAMPLER", &samplerDesc)};

  // Phong & Unlit Default Surface Shader Effect
  {
    rtDescription_surfaceSE sseDesc;
    sseDesc.WPO_code = nullptr;
    sseDesc.shading_code =
      "vec4 surface_shading(){ return sample_DIFFUSETEXTURE(0,) / vec4(255.0f); }";
    sseDesc.attribCount        = 1;
    sseDesc.attribNames        = &attribNames[1];
    sseDesc.dataTypes          = &attribElementTypesTgfx[1];
    sseDesc.inputRates         = &attribInputRate;
    sseDesc.instanceInputCount = 2;
    sseDesc.instanceInputs     = difInputs;
    sseDesc.language           = shaderlanguages_tgfx_GLSL;
    defaultUnlit = SSEM_createEffect(&sseDesc);

    sseDesc.shading_code =
      "vec4 surface_shading(){ return vec4(texture(usampler2D(DIFFUSETEXTURE, DIFFUSESAMPLER), "
                                              "UVCOORD)) / vec4(255.0f); }";
    defaultPhongLit = SSEM_createEffect(&sseDesc);
  }
}
void initializeSurfaceRenderer() {
  {
    rtSurfaceRenderer* r           = new rtSurfaceRenderer;
    r->createMaterial              = SMM_createMaterial;
    r->createPhongMaterialInstance = SMM_createPhongMaterialInstance;
    r->draw                        = draw;
    r->registerRenderer            = registerRenderer;
    surfaceRenderer                = r;
  }

  // Create unlit default material
  {

  }

  // Create phong default material
  {}
}