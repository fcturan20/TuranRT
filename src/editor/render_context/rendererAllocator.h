#pragma once
typedef struct gpuMemRegion_rt* rtGpuMemRegion;
// Memory block is to describe a resource's memory location
// Memory blocks may contain other memory blocks, so resources can be suballocated
// For example: gpuVertexBuffer is created for mesh rendering and bind to a memory region
//  We want to suballocate each mesh's vertex buffer from this main gpuVertexBuffer memory block.
//  So each rtGpuMeshBuffer is a suballocated rtGpuMemBlock.
typedef struct gpuMemBlock_rt {
  // If isMapped is false, uploading is automatically handled by allocating a staging buffer
  bool            isActive = false, isMapped = false, isResourceBuffer = false;
  rtGpuMemRegion memoryRegion;
  void*           resource;
  uint64_t        offset, size;
} gpuMemBlock_rt;
rtGpuMemBlock allocateBuffer(uint64_t size, bufferUsageMask_tgfxflag usageFlag,
                             rtGpuMemRegion memRegion = nullptr);
rtGpuMemBlock allocateTexture(const textureDescription_tgfx& desc,
                              rtGpuMemRegion                memRegion = nullptr);
rtGpuMemRegion getGpuMemRegion(rtRenderer::regionType memType);
void                            initMemRegions();