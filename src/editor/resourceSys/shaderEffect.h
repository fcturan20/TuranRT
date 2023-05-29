#pragma once
#ifdef __cplusplus
extern "C" {
#endif

// Acronyms: SE -> ShaderEffect, SEmt -> ShaderEffect Manager Type
// SEI -> ShaderEffect Instance: Stores uniforms & references (no code changes)
// SEIs will be created by rtSEM.

// * This system is to provide all GPU-side shader based operations (graphics, compute, ray-tracing)
// * 2-level hierarchy is maintained for easier implementations: SE & SEI
// * SE: General purpose (maybe specific to a gpu-stage) code is stored with resource-access way
// defined
//    * It's upto renderer to manage SEmt specific resource-accesses (like vertex attributes)
// * SEI: As SE stores its instance resource-access, it is possible to reference different resources
// (buffers, textures, pass-time uniforms)
// * For example: Surface SE will store vertex attribute names & shading code.
//     Then ForwardMesh renderer'll compile a raster-pipeline if attribute names're matching
//     with the ones it supports. Then user'll create SEI to set texture. After using setSEI_XXX(),
//     SEI can be passed to ForwardMesh to be rendered with the mesh it's associated with.
//     But VisibilityMesh renderer'll compile a frag-shader with matching attribute data loading
//     shader funcs and an shaderID to store in MaterialMask-RT at VisibilityPass.

////////// USER INTERFACE

struct rtShaderEffect*         SEM_createSE(const struct rtShaderEffectManagerType* Mmt,
                                                   void*                                   extraInfo);
struct rtShaderEffectInstance* SEM_createSEI(struct rtShaderEffect* effect, void* extraInfo);
unsigned char SEM_destroySEs(unsigned int count, struct rtShaderEffect** shaderEffects);
unsigned char SEM_destroySEIs(unsigned int                    count,
                                     struct rtShaderEffectInstance** shaderEffectInstances);
void          SEM_frame();

struct rtShaderEffectInstanceInput* SEM_createSEIInput_buffer(const char*        name,
                                                                     unsigned long long size);
struct rtShaderEffectInstanceInput* SEM_createSEIInput_texture(
  const char* name, unsigned char isSampled, enum textureChannels_tgfx channelInfo);

void SEM_setSEI_buffer(struct rtShaderEffectInstance*      instance,
                              struct rtShaderEffectInstanceInput* info, struct tgfx_buffer_obj* buf,
                              unsigned long long offset, unsigned long long size);
void SEM_setSEI_texture(struct rtShaderEffectInstance*      instance,
                               struct rtShaderEffectInstanceInput* info,
                               struct tgfx_texture_obj*            texture);

struct rtShaderEffect* SEM_getSE(const struct rtShaderEffectInstance* instance);
void                   SEM_getBindingTableDesc(struct rtShaderEffect*                    shader,
                                                      const struct rtShaderEffectInstanceInput* instanceInput,
                                                      struct tgfx_binding_table_description*    desc);

///////////////////////////

///////////// ShaderEffect Manager Type (SEmt) definitions (Only for SEmt implementors)

// Extra info is manager specific data input (like vertex attribute info in visibilityMesh)
struct SEM_managerDesc {
  const char*  managerName;
  unsigned int managerVer;
  unsigned int shaderEffectStructSize;
  struct rtShaderEffect* (*createShaderEffectFnc)(void* extraInfo);
  unsigned char (*destroyShaderEffectFnc)(struct rtShaderEffect* sfx);
  // Function should always return info count. Infos can be nullptr to detect count.
  unsigned int (*getRtShaderEffectInstanceInfoFnc)(struct rtShaderEffect*               sfx,
                                                   struct rtShaderEffectInstanceInput** infos);
  void (*frameFnc)();
};
// Only for SEmt implementors
const struct rtShaderEffectManagerType* SEM_registerManager(SEM_managerDesc desc);
// Only for SEmt implementors
void* SEM_createShaderEffectHandle(const struct rtShaderEffectManagerType* managerType);

////////////////////////////

// Resource Manager Type (Rmt) implementation
const struct rtResourceManagerType* SEM_managerType();
void                                SEM_initializeManager();

#ifdef __cplusplus
}
#endif