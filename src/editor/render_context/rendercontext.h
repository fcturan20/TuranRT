#pragma once
#include <tgfx_structs.h>
static constexpr unsigned int swapchainTextureCount = 2;
// Include tgfx_forwardDeclarations.h & glm/glm.hpp before including this

typedef struct mat4_rt                          rtMat4;
typedef struct gpuMemBlock_rt*                  rtGpuMemBlock;
typedef struct tgfx_raster_pipeline_description rasterPipelineDescription_tgfx;
struct rtRenderer {
  enum regionType { UNDEF, UPLOAD, LOCAL, READBACK };
  static rtGpuMemBlock allocateMemoryBlock(bufferUsageMask_tgfxflag flag, uint64_t size,
                                           regionType memType = UNDEF);
  static void          deallocateMemoryBlock(rtGpuMemBlock memBlock);
  static void          setActiveFrameCamProps(const rtMat4* view, const rtMat4* proj);

  // Renderer execute uploadBundle at the beginning of the frame.
  // No caching, so user should call it every frame.
  // User should manage command bundle lifetime (record, destroy)
  static void upload(commandBundle_tgfxhnd uploadBundle);
  // Similar to upload but executed after all upload calls finished
  static void render(commandBundle_tgfxhnd renderBundle);

  static void initialize(tgfx_windowKeyCallback keyCB);
  static void getSwapchainTexture();
  static void renderFrame();
  static void close();

  // Block should be allocated with either UPLOAD or READBACK
  static void*                getBufferMappedMemPtr(rtGpuMemBlock block);
  static buffer_tgfxhnd       getBufferTgfxHnd(rtGpuMemBlock block);
  static shaderSource_tgfxhnd getDefaultFragShader();
  static void                 getRTFormats(rasterPipelineDescription_tgfx* rasterPipeDesc);
  static unsigned int         getFrameIndx();
  static bindingTable_tgfxhnd getActiveCamBindingTable();
  static const bindingTableDescription_tgfx* getCamBindingDesc();
  static tgfx_uvec2                          getResolution();
  static tgfx_gpu_description                getGpuDesc();
};
extern gpuQueue_tgfxhnd allQueues[TGFX_WINDOWGPUSUPPORT_MAXQUEUECOUNT];

struct gpuStorageBuffer_rt;
typedef struct gpuStorageBuffer_rt* rtGpuStorageBuffer;
typedef struct storagerenderer_rt {
} rtStorageRenderer;

extern window_tgfxhnd mainWindowRT;