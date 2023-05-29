#pragma once
#ifdef __cplusplus
extern "C" {
#endif

// Memory block is to describe a resource's memory location
// Memory blocks may contain other memory blocks, so resources can be suballocated
// For example: gpuVertexBuffer is created for mesh rendering and bind to a memory region
//  We want to suballocate each mesh's vertex buffer from this main gpuVertexBuffer memory block.
//  So each rtGpuMeshBuffer is a suballocated rtGpuMemBlock.
typedef struct rtGpuMemBlock {
  // If isMapped is false, uploading is automatically handled by allocating a staging buffer
  bool           isActive = false, isMapped = false, isResourceBuffer = false;
  struct rtGpuMemRegion* memoryRegion;
  void*          resource;
  uint64_t       offset, size;
} rtGpuMemBlock;
struct rtGpuMemBlock*  allocateBuffer(uint64_t size, bufferUsageMask_tgfxflag usageFlag,
                              struct rtGpuMemRegion* memRegion);
struct rtGpuMemBlock*  allocateTexture(const textureDescription_tgfx& desc,
                               struct rtGpuMemRegion*                 memRegion);
struct rtGpuMemRegion* getGpuMemRegion(enum rtMemoryRegionType memType);
void           initMemRegions();

#ifdef __cplusplus
}
#endif