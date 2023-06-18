#pragma once
#ifdef __cplusplus
extern "C" {
#endif
#include <tgfx_structs.h>
static constexpr unsigned int swapchainTextureCount = 2;
// Include tgfx_forwardDeclarations.h, tgfx_structs.h & glm.hpp before including

extern struct tgfx_gpuQueue* allQueues[64];

struct rtRendererDescription {
  void* data;
  void (*renderFrame)(void* data);
  void (*close)(void* data);
};
struct rtGpuCopyInfo {
  struct tgfx_heap* heap;
  unsigned int      size, heapOffset;
  // If region is UPLOAD or READBACK, use this for CPU access
  void* mappedLocation;
};
struct rtGpuMemoryRegion;
struct rtRenderer {
  //////////////// GENERIC SYSTEM FUNCTIONALITY:

  // Use this for independent renderers
  // For example: 3D WorldRenderer (all post-fx etc), lightmap renderer etc.
  struct teRendererInstance* (*registerRendererInstance)(const struct rtRendererDescription* desc);
  void (*renderFrame)();
  void (*close)();

  //////////////// SHADER/PIPELINE COMPILATION:

  /* Until TESL & RenderPass is implemented, GLSL is used
   * Push constants are 128 bytes for all pipelines
   * Backend should add a shader that implements push constant struct
   * Vertex attributes should be loaded by the user with binding tables
   */
  struct teShader* (*compileShader)(const char* code);
  struct tgfx_pipeline* (*createRasterPipeline)(unsigned int            vertexShaderCount,
                                                struct teShader* const* vertexShaders,
                                                unsigned int            fragmentShaderCount,
                                                struct teShader* const* fragmentShaders,
                                                textureChannels_tgfx    colorFormats[8],
                                                textureChannels_tgfx    depthFormat);
  struct tgfx_pipeline* (*createComputePipeline)(unsigned int            shaderCount,
                                                 struct teShader* const* computeShaders);

  //////////////// SHADER/PIPELINE INPUT BINDING MANAGEMENT

  // @return Allocated binding ID
  unsigned int (*allocateBinding)(enum shaderdescriptortype_tgfx bindingType);
  void (*bindGlobalBindingTables)(struct tgfx_commandBundle* bundle);
  void (*bindTexture)(unsigned int bindingID, unsigned char isSampled,
                      struct tgfx_texture* texture);
  void (*bindSampler)(unsigned int bindingID, struct tgfx_sampler* sampler);
  void (*bindBuffer)(unsigned int bindingID, struct tgfx_buffer* buffer, unsigned int offset,
                     unsigned int size);

  //////////////// MEMORY ALLOCATION

  // If size is UINT64_MAX, region'll use dynamic allocation
  // It's better to specify the size, because static allocations're faster
  // NOTE: Some devices may have special regions only for special resources.
  //   To handle such cases, you better pass some of your resource example requirements
  //   This is why memory regions are user-created
  // For example; If GPU stores sampled textures in a special region,
  //   heap allocated from gpuTextureManager'll be different than mesh heap etc.
  struct rtGpuMemoryRegion* (*createRegion)(enum memoryallocationtype_tgfx regionType,
                                            unsigned long long size, unsigned int reqsCount,
                                            const struct tgfx_heapRequirementsInfo* reqs);
  void (*destroyRegion)(struct rtGpuMemoryRegion* region);
  struct rtGpuCopyInfo (*allocateSize)(struct rtGpuMemoryRegion*               region,
                                       const struct tgfx_heapRequirementsInfo* req);
  // User should return the same struct returned from allocateSize
  void (*deallocateSize)(struct rtGpuMemoryRegion* region, const struct rtGpuCopyInfo* info);

  ///////////////// GETTER-SETTERs

  // Use this to resize your render targets, buffers etc.
  const struct tgfx_textureDescription* (*getSwapchainInfo)();
  unsigned int (*getFrameIndx)();
};
extern const struct rtRenderer* renderer;
void                     initializeRenderer(tgfx_windowKeyCallback keyCB);

extern struct tgfx_window* mainWindowRT;
#ifdef __cplusplus
}
#endif