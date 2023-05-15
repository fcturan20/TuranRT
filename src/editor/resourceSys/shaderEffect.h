#pragma once

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

typedef struct shaderEffect_rt*             rtShaderEffect;
typedef struct shaderEffectInstance_rt*     rtShaderEffectInstance;
typedef struct shaderEffectManagerType_rt*  rtShaderEffectManagerType;
typedef struct shaderEffectInstanceInfo_rt* rtShaderEffectInstanceInfo;
typedef struct shaderEffectManager_rt {
  ////////// USER INTERFACE

  static rtShaderEffect             createSE(rtShaderEffectManagerType Mmt, void* extraInfo);
  static rtShaderEffectInstance     createSEI(rtShaderEffect effect, void* extraInfo);
  static unsigned char              destroySEs(unsigned int count, rtShaderEffect* shaderEffects);
  static unsigned char              destroySEIs(unsigned int            count,
                                                rtShaderEffectInstance* shaderEffectInstances);
  static void                       frame();
  static rtShaderEffectInstanceInfo createSEIInfo_buffer(const char* name, unsigned long long size);
  static rtShaderEffectInstanceInfo createSEIInfo_texture(const char* name, unsigned char isSampled,
                                                          textureChannels_tgfx channelInfo);

  static void setSEI_buffer(rtShaderEffectInstance instance, rtShaderEffectInstanceInfo info,
                            buffer_tgfxhnd buf, unsigned long long offset, unsigned long long size);
  static void setSEI_texture(rtShaderEffectInstance instance, rtShaderEffectInstanceInfo info,
                             texture_tgfxhnd texture);

  ///////////////////////////

  ///////////// ShaderEffect Manager Type (SEmt) definitions (Only for SEmt implementors)

  // Extra info is manager specific data input (like vertex attribute info in visibilityMesh)
  struct managerDesc {
    const char* managerName;
    uint32_t    managerVer;
    uint32_t    shaderEffectStructSize;
    rtShaderEffect (*createShaderEffectFnc)(void* extraInfo);
    unsigned char (*destroyShaderEffectFnc)(rtShaderEffect sfx);
    // Function should always return info count. Infos can be nullptr to detect count.
    unsigned int (*getRtShaderEffectInstanceInfoFnc)(rtShaderEffect              sfx,
                                                     rtShaderEffectInstanceInfo* infos);
    void (*frameFnc)();
  };
  // Only for SEmt implementors
  static rtShaderEffectManagerType registerManager(managerDesc desc);
  // Only for SEmt implementors
  static void* createShaderEffectHandle(rtShaderEffectManagerType managerType);

  ////////////////////////////

  // Resource Manager Type (Rmt) implementation
  typedef rtShaderEffect       defaultResourceType;
  static rtResourceManagerType managerType();
  static void                  initializeManager();
} rtShaderEffectManager; // rtSEM