#pragma once
#ifdef __cplusplus
extern "C" {
#endif

// Include resourceManager.h, mesh.h & tgfx_forwarddeclarations.h before this

/*
Creates HDR surface shading effect that can be called in shader with calling
  surface_shading()
Main body of shading code should be in "vec4 surface_shading(){}"
If world position's gonna change, "vec3 calculate_wpo(){}" should be implemented
Descriptor set indexes: 0 -> Sampler, 1 -> SampledTexture, 2 -> ImageTexture, 3 -> Buffer
All shader buffer inputs should be implemented as array-of-structs like this:
  struct material{
    uint diffuseTextureID, specularTextureID;
  };
  layout(set = 3, binding = 0) readonly buffer materialBuffer{
    material materials[];
  } materialBuffer[]; // All types of input buffers use the same binding
  material getMaterial(){
    return materialBuffer[pushConstants.pipelineTypeID].materials[pushConstants.materialID];
  }
*/

// Surface renderer should implement these functions for full compatibility:
// vec2 get_derivatives();
// uint get_materialID();
// vecX get_"AttribName"();
// vec2 get_fragCoord();

// Pass an object of this struct to createEffect func
struct rtSurfaceMaterialDescription {
  const char  *shadingCode;
  const char  *wpoCode;
  const char **attribNames;
  unsigned int attribCount;
};
struct rtSurfaceRenderingDrawCall {
  struct rtMesh            *mesh;
  struct rtSurfaceMaterial *material;
  unsigned int              instanceBufferID;
};
struct rtSurfaceRendererDescription {
  struct rtSurfaceRendererType *typeHandle;
  // @return 1: Surface renderer can use this material, 0: it can't use this material
  unsigned char (*compileSurfacePipeline)(const struct rtSurfaceMaterialDescription *desc);
  void (*destroySurfacePipeline)(const struct rtSurfaceMaterial *mat);
};



struct rtPhongMaterialBuffer {
  unsigned int diffuseTextureID, samplerID;
  struct tgfx_vec4 diffuseColor;
};
struct rtSurfaceRenderer {
  ////////// RENDERING

  void (*registerRenderer)(const struct rtSurfaceRendererDescription *desc);
  void (*draw)(struct rtSurfaceRendererType *renderer, unsigned int drawCallCount,
               const struct rtSurfaceRenderingDrawCall *drawCalls);

  
  ////////// MATERIAL MANAGEMENT

  struct rtSurfaceMaterial *(*createMaterial)(const struct rtSurfaceMaterialDescription *desc);
  // Create default surface shader effects (phong-lit, unlit)
  struct rtPhongMaterialBuffer *(*createPhongMaterialInstance)();
};
void                             initializeSurfaceRenderer();
extern const struct rtSurfaceRenderer *surfaceRenderer;

#ifdef __cplusplus
}
#endif