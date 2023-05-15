#pragma once
// Include resourceManager.h & tgfx_forwarddeclarations.h before this

// Creates HDR surface shading effect that can be called in shader with calling surface_shading()
// Main body of shading code should be in "vec4 surface_shading(){}".

typedef struct surfaceShaderEffect_rt *rtSurfaceShaderEffect;
// Pass an object of this struct to createEffect func
struct rtDescription_surfaceSE {
  shaderlanguages_tgfx            language;
  const char                     *shading_code;
  const char                     *WPO_code;
  unsigned int                    attribCount;
  const char                    **attribNames;
  datatype_tgfx                  *dataTypes;
  vertex_binding_input_rate_tgfx *inputRates;
  struct attrib_rt {
    const char                    *attribName = nullptr;
    datatype_tgfx                  dataType   = datatype_tgfx_UNDEFINED;
    vertex_binding_input_rate_tgfx inputRate  = vertexBindingInputRate_tgfx_UNDEF;
  };
  uint32_sc surfaceMat_maxAttribCount = 6;
  attrib_rt attribs[surfaceMat_maxAttribCount];
};
typedef struct surfaceShaderEffectManager_rt {
  ////////// USER INTERFACE

  static rtSurfaceShaderEffect   createEffect(const rtDescription_surfaceSE *desc);
  static rtDescription_surfaceSE getSurfaceSEProps(rtSurfaceShaderEffect surfaceSfx);

  ////////////////////////////

  // Resource Manager Type (Rmt) implementation
  typedef rtSurfaceShaderEffect    defaultShaderEffectType;
  static rtShaderEffectManagerType managerType();
  static void                      initializeManager();
} rtSurfaceMaterialManager;