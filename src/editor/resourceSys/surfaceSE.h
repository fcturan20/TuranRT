#pragma once
#ifdef __cplusplus
extern "C" {
#endif

// Include resourceManager.h, shaderEffect.h, mesh.h & tgfx_forwarddeclarations.h before this

// Creates HDR surface shading effect that can be called in shader with calling surface_shading()
// Main body of shading code should be in "vec4 surface_shading(){}".

// Pass an object of this struct to createEffect func
typedef struct rtDescription_surfaceSE {
  shaderlanguages_tgfx                       language;
  const char                                *shading_code;
  const char                                *WPO_code;
  unsigned int                               attribCount;
  const char                               **attribNames;
  const datatype_tgfx                       *dataTypes;
  const vertex_binding_input_rate_tgfx      *inputRates;
  const struct rtShaderEffectInstanceInput **inputs;
  unsigned int                               instanceInputCount;
} rtDescription_surfaceSE;

////////// USER INTERFACE

struct rtSurfaceShaderEffect  *SSEM_createEffect(const struct rtDescription_surfaceSE *desc);
struct rtDescription_surfaceSE SSEM_getSurfaceSEProps(struct rtSurfaceShaderEffect *surfaceSfx);
void SSEM_addPipeline(struct rtSurfaceShaderEffect *surfaceSfx, pipeline_tgfxhnd pipeline,
                      const struct rtMeshManagerType *meshManagerType);
pipeline_tgfxhnd SSEM_getPipeline(struct rtSurfaceShaderEffect   *surfaceSfx,
                                  const struct rtMeshManagerType *meshManagerType);

////////////////////////////

// Resource Manager Type (Rmt) implementation
const struct rtShaderEffectManagerType *SSEM_managerType();

void SSEM_initializeManager();

#ifdef __cplusplus
}
#endif