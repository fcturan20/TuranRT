#pragma once
// Include tgfx_forwardDeclarations.h & glm/glm.hpp before including this

struct gpuMemBlock_rt;
typedef struct gpuMemBlock_rt* rtGpuMemBlock;

struct rtRenderer {
  enum regionType { UNDEF, UPLOAD, LOCAL, READBACK };
  static rtGpuMemBlock allocateMemoryBlock(bufferUsageMask_tgfxflag flag, uint64_t size,
                                           regionType memType = UNDEF);
  // Block should be allocated with either UPLOAD or READBACK
  static void*          getBufferMappedMemPtr(rtGpuMemBlock block);
  static buffer_tgfxhnd getBufferTgfxHnd(rtGpuMemBlock block);
  static void           deallocateMemoryBlock(rtGpuMemBlock memBlock);
  static void           setActiveFrameCamProps(glm::mat4 view, glm::mat4 proj);
  static void           initialize(tgfx_windowKeyCallback keyCB);
  static void           renderFrame();
  // Renderer execute uploadBundle at the beginning of the frame.
  // No caching, so user should call it every frame.
  // User should manage command bundle lifetime (record, destroy)
  static void upload(commandBundle_tgfxhnd uploadBundle);
  static void close();
};

struct gpuStorageBuffer_rt;
typedef struct gpuStorageBuffer_rt* rtGpuStorageBuffer;
typedef struct storagerenderer_rt {
} rtStorageRenderer;