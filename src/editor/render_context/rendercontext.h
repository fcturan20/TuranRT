#pragma once
#ifdef __cplusplus
extern "C" {
#endif
static constexpr unsigned int swapchainTextureCount = 2;
// Include tgfx_forwardDeclarations.h, tgfx_structs.h, shaderEffect.h & glm/glm.hpp before including
// this

typedef struct mat4_rt                          rtMat4;
typedef struct tgfx_raster_pipeline_description rasterPipelineDescription_tgfx;
typedef enum rtMemoryRegionType {
  rtMemoryRegion_UNDEF,
  rtMemoryRegion_UPLOAD,
  rtMemoryRegion_LOCAL,
  rtMemoryRegion_READBACK
} rtMemoryRegionType;
struct rtRenderer {
  static struct rtGpuMemBlock* allocateMemoryBlock(bufferUsageMask_tgfxflag flag, uint64_t size,
                                            enum rtMemoryRegionType memType);
  static void           deallocateMemoryBlock(struct rtGpuMemBlock* memBlock);
  static void           setActiveFrameCamProps(const rtMat4* view, const rtMat4* proj,
                                               const tgfx_vec3* camPos, const tgfx_vec3* camDir,
                                               float fovDegrees);

  // Renderer execute uploadBundle at the beginning of the frame.
  // No caching, so user should call it every frame.
  // User should manage command bundle lifetime (record, destroy)
  static void upload(commandBundle_tgfxhnd uploadBundle);
  // Similar to upload but executed after all upload calls finished
  static void rasterize(commandBundle_tgfxhnd renderBundle);
  // Similar to rasterize but executed after all rasterize calls finished
  static void compute(commandBundle_tgfxhnd commandBundle);

  static void initialize(tgfx_windowKeyCallback keyCB);
  static void getSwapchainTexture();
  static void renderFrame();
  static void close();

  // Block should be allocated with either UPLOAD or READBACK
  static void*                getBufferMappedMemPtr(struct rtGpuMemBlock* block);
  static buffer_tgfxhnd       getBufferTgfxHnd(struct rtGpuMemBlock* block);
  static void                 getRTFormats(rasterPipelineDescription_tgfx* rasterPipeDesc);
  static unsigned int         getFrameIndx();
  static bindingTable_tgfxhnd getActiveCamBindingTable();
  static bindingTable_tgfxhnd getSwapchainStorageBindingTable();
  static const bindingTableDescription_tgfx* getCamBindingDesc();
  static const bindingTableDescription_tgfx* getSwapchainStorageBindingDesc();
  static tgfx_uvec2                          getResolution();
  static const struct tgfx_gpu_description*  getGpuDesc();
  static struct rtShaderEffect*              getDefaultSurfaceShaderFX();
};
extern gpuQueue_tgfxhnd allQueues[64];

struct gpuStorageBuffer_rt;
typedef struct gpuStorageBuffer_rt* rtGpuStorageBuffer;
typedef struct storagerenderer_rt {
} rtStorageRenderer;

extern window_tgfxhnd mainWindowRT;
#ifdef __cplusplus
}
#endif