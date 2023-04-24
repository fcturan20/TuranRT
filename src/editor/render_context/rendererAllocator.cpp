#include <glm/glm.hpp>
#include <vector>

#include <tgfx_core.h>
#include <tgfx_gpucontentmanager.h>
#include <tgfx_helper.h>
#include <tgfx_renderer.h>
#include <tgfx_structs.h>

#include "../editor_includes.h"
#include "rendercontext.h"
#include "rendererAllocator.h"

rtGpuMemRegion m_devLocalAllocations = {}, m_stagingAllocations = {};
// Returns the dif between offset's previous and new value
uint64_t calculateOffset(uint64_t* lastOffset, uint64_t offsetAlignment) {
  uint64_t prevOffset = *lastOffset;
  *lastOffset =
    ((*lastOffset / offsetAlignment) + ((*lastOffset % offsetAlignment) ? 1 : 0)) * offsetAlignment;
  return *lastOffset - prevOffset;
};
// Memory region is a memoryHeap_tgfx that a resource can be bound to
struct gpuMemRegion_rt {
  std::vector<rtGpuMemBlock> memAllocations;
  uint64_t                   memoryRegionSize = 0;
  heap_tgfxhnd               memoryHeap       = {};
  void*                      mappedRegion     = {};
  uint32_t                   memoryTypeIndx   = 0;

  rtGpuMemBlock        allocateMemBlock(heapRequirementsInfo_tgfx reqs);
};

static rtGpuMemBlock findRegionAndAllocateMemBlock(rtGpuMemRegion*           memRegion,
                                                   heapRequirementsInfo_tgfx reqs) {
  auto allocate = [reqs](rtGpuMemRegion memRegion) -> rtGpuMemBlock {
    rtGpuMemBlock memBlock = memRegion->allocateMemBlock(reqs);
    if (!memBlock) {
      return nullptr;
    }
    return memBlock;
  };

  rtGpuMemBlock block = {};
  if (*memRegion) {
    block = allocate(*memRegion);
  }

  if (reqs.memoryRegionIDs[m_devLocalAllocations->memoryTypeIndx] && !block) {
    *memRegion = m_devLocalAllocations;
    block      = allocate(*memRegion);
  }
  if (reqs.memoryRegionIDs[m_stagingAllocations->memoryTypeIndx] && !block) {
    *memRegion = m_stagingAllocations;
    block      = allocate(*memRegion);
  }
  return block;
}
rtGpuMemBlock allocateBuffer(uint64_t size, bufferUsageMask_tgfxflag usageFlag,
                             rtGpuMemRegion memRegion) {
  buffer_tgfxhnd         buf        = {};
  bufferDescription_tgfx bufferDesc = {};
  bufferDesc.dataSize               = size;
  bufferDesc.exts                   = {};
  bufferDesc.permittedQueues        = allQueues;
  bufferDesc.usageFlag              = usageFlag;
  contentManager->createBuffer(gpu, &bufferDesc, &buf);
  tgfx_heap_requirements_info reqs;
  contentManager->getHeapRequirement_Buffer(buf, 0, {}, &reqs);

  rtGpuMemBlock memBlock = {};
  memBlock               = findRegionAndAllocateMemBlock(&memRegion, reqs);
  if (memBlock) {
    memBlock->isResourceBuffer = true;
    memBlock->resource         = buf;
    if (contentManager->bindToHeap_Buffer(memRegion->memoryHeap, memBlock->offset, buf, 0, {}) !=
        result_tgfx_SUCCESS) {
      assert(0 && "Bind to Heap failed!");
    }
  }

  return memBlock;
}
rtGpuMemBlock allocateTexture(const textureDescription_tgfx& desc,
                              rtGpuMemRegion                memRegion) {
  texture_tgfxhnd tex;
  contentManager->createTexture(gpu, &desc, &tex);
  heapRequirementsInfo_tgfx reqs;
  contentManager->getHeapRequirement_Texture(tex, 0, nullptr, &reqs);

  rtGpuMemBlock memBlock     = {};
  memBlock                   = findRegionAndAllocateMemBlock(&memRegion, reqs);
  memBlock->isResourceBuffer = false;
  memBlock->resource         = tex;
  if (contentManager->bindToHeap_Texture(memRegion->memoryHeap, memBlock->offset, tex, 0,
                                         nullptr) != result_tgfx_SUCCESS) {
    assert(0 && "Bind to Heap failed!");
  }
  return memBlock;
}
rtGpuMemRegion getGpuMemRegion(rtRenderer::regionType memType) {
  switch (memType) {
    case rtRenderer::UPLOAD: return m_stagingAllocations;
    case rtRenderer::LOCAL: return m_devLocalAllocations;
  }
  return nullptr;
}
rtGpuMemBlock allocateMemBlock(std::vector<rtGpuMemBlock>& memAllocs,
                                                uint64_t regionSize, uint64_t size,
                                                uint64_t alignment) {
  uint64_t maxOffset = 0;
  for (rtGpuMemBlock memBlock : memAllocs) {
    maxOffset = glm::max(maxOffset, memBlock->offset + memBlock->size);
    if (memBlock->isActive || memBlock->size < size) {
      continue;
    }

    uint64_t memBlockOffset = memBlock->offset;
    uint64_t dif            = calculateOffset(&memBlockOffset, alignment);
    if (memBlock->size < dif + size) {
      continue;
    }

    return memBlock;
  }

  calculateOffset(&maxOffset, alignment);
  if (maxOffset + size > regionSize) {
    printf("Memory region size is exceeded!");
    return nullptr;
  }

  rtGpuMemBlock memBlock = new gpuMemBlock_rt;
  memBlock->isActive     = true;
  memBlock->offset       = maxOffset;
  memBlock->size         = size;
  memAllocs.push_back(memBlock);
  return memBlock;
}
rtGpuMemBlock rtRenderer::allocateMemoryBlock(bufferUsageMask_tgfxflag flag, uint64_t size,
                                              regionType memType) {
  rtGpuMemRegion region = getGpuMemRegion(memType);
  return allocateBuffer(size, flag, region);
}
void rtRenderer::deallocateMemoryBlock(rtGpuMemBlock memBlock) {
  if (memBlock->isResourceBuffer) {
    contentManager->destroyBuffer(( buffer_tgfxhnd )memBlock->resource);
  } else {
    contentManager->destroyTexture(( texture_tgfxhnd )memBlock->resource);
  }

  memBlock->isActive = false;
  memBlock->resource = nullptr;
  memBlock->isResourceBuffer = false;
}
rtGpuMemBlock gpuMemRegion_rt::allocateMemBlock(heapRequirementsInfo_tgfx reqs) {
  rtGpuMemBlock memBlock =
    ::allocateMemBlock(memAllocations, memoryRegionSize, reqs.size, reqs.offsetAlignment);
  if (!memBlock) {
    return nullptr;
  }
  memBlock->isMapped     = mappedRegion ? true : false;
  memBlock->memoryRegion = this;
  return memBlock;
}
void*         rtRenderer::getBufferMappedMemPtr(rtGpuMemBlock block) {
  if (!block->isMapped) {
    return nullptr;
  }
  return ( void* )(( uintptr_t )block->memoryRegion->mappedRegion + block->offset);
}
buffer_tgfxhnd rtRenderer::getBufferTgfxHnd(rtGpuMemBlock block) {
  return block->isResourceBuffer ? ( buffer_tgfxhnd )block->resource : nullptr;
}
void initMemRegions() {
  const tgfx_gpu_description& gpuDesc            = rtRenderer::getGpuDesc();
  static constexpr uint32_t   heapSize           = 1 << 27;
  uint32_t                    deviceLocalMemType = UINT32_MAX, hostVisibleMemType = UINT32_MAX;
  for (uint32_t memTypeIndx = 0; memTypeIndx < gpuDesc.memRegionsCount; memTypeIndx++) {
    const memoryDescription_tgfx& memDesc = gpuDesc.memRegions[memTypeIndx];
    if (memDesc.allocationType == memoryallocationtype_HOSTVISIBLE ||
        memDesc.allocationType == memoryallocationtype_FASTHOSTVISIBLE) {
      // If there 2 different memory types with same allocation type, select the bigger one!
      if (hostVisibleMemType != UINT32_MAX &&
          gpuDesc.memRegions[hostVisibleMemType].maxAllocationSize > memDesc.maxAllocationSize) {
        continue;
      }
      hostVisibleMemType = memTypeIndx;
    }
    else if (memDesc.allocationType == memoryallocationtype_DEVICELOCAL) {
      // If there 2 different memory types with same allocation type, select the bigger one!
      if (deviceLocalMemType != UINT32_MAX &&
          gpuDesc.memRegions[deviceLocalMemType].maxAllocationSize > memDesc.maxAllocationSize) {
        continue;
      }
      deviceLocalMemType = memTypeIndx;
    }
  }
  assert(hostVisibleMemType != UINT32_MAX && deviceLocalMemType != UINT32_MAX &&
         "An appropriate memory region isn't found!");


  // Create Host Visible (staging) memory heap
  m_stagingAllocations = new gpuMemRegion_rt;
  contentManager->createHeap(gpu, gpuDesc.memRegions[hostVisibleMemType].memoryTypeId, heapSize, 0,
                             nullptr, &m_stagingAllocations->memoryHeap);
  contentManager->mapHeap(m_stagingAllocations->memoryHeap, 0, heapSize, 0,
                          nullptr, &m_stagingAllocations->mappedRegion);
  m_stagingAllocations->memoryRegionSize = heapSize;
  m_stagingAllocations->memoryTypeIndx   = hostVisibleMemType;
  // Create Device Local memory heap
  m_devLocalAllocations = new gpuMemRegion_rt;
  contentManager->createHeap(gpu, gpuDesc.memRegions[deviceLocalMemType].memoryTypeId, heapSize, 0,
                             nullptr, &m_devLocalAllocations->memoryHeap);
  m_devLocalAllocations->memoryTypeIndx = deviceLocalMemType;
  m_devLocalAllocations->memoryRegionSize = heapSize;
}

typedef struct gpuResourceType_rt {
  int resourceTypeIndx;
  void (*allocateCallback)(rtGpuMemBlock allocatedRegion);
} rtGpuResourceType;