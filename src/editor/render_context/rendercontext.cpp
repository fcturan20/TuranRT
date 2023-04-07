#include <glm/glm.hpp>
#include <vector>
#include <string.h>

#include <filesys_tapi.h>
#include <string_tapi.h>
#include <profiler_tapi.h>

#include <tgfx_core.h>
#include <tgfx_gpucontentmanager.h>
#include <tgfx_helper.h>
#include <tgfx_renderer.h>
#include <tgfx_structs.h>

#include "../editor_includes.h"
#include "rendercontext.h"

tgfx_renderer*            renderer       = nullptr;
tgfx_gpudatamanager*      contentManager = nullptr;
gpu_tgfxhnd               gpu            = nullptr;
tgfx_gpu_description      gpuDesc;
textureUsageMask_tgfxflag textureAllUsages = textureUsageMask_tgfx_COPYFROM |
                                             textureUsageMask_tgfx_COPYTO |
                                             textureUsageMask_tgfx_RANDOMACCESS |
                                             textureUsageMask_tgfx_RASTERSAMPLE |
                                             textureUsageMask_tgfx_RENDERATTACHMENT,
                          storageImageUsage =
                            textureUsageMask_tgfx_COPYTO | textureUsageMask_tgfx_RANDOMACCESS;
gpuQueue_tgfxhnd           allQueues[TGFX_WINDOWGPUSUPPORT_MAXQUEUECOUNT] = {};
window_tgfxhnd             mainWindowRT;
tgfx_swapchain_description swpchn_desc;
tgfx_window_gpu_support    swapchainSupport                      = {};
textureChannels_tgfx       depthRTFormat                         = texture_channels_tgfx_D24S8;
texture_tgfxhnd            swpchnTextures[swapchainTextureCount] = {};
// Create device local resources
pipeline_tgfxhnd             firstComputePipeline = {};
bindingTable_tgfxhnd         bufferBindingTable   = {};
uint32_t                     camUboOffset         = {};
uint64_t                     waitValue = 0, signalValue = 1;
fence_tgfxhnd                fence               = {};
gpuQueue_tgfxhnd             queue               = {};
rasterpassBeginSlotInfo_tgfx colorAttachmentInfo = {}, depthAttachmentInfo = {};
static uint32_t              GPU_INDEX = 0;

typedef struct gpuMemBlock_rt* rtGpuMemBlock;
// Returns the dif between offset's previous and new value
uint64_t calculateOffset(uint64_t* lastOffset, uint64_t offsetAlignment) {
  uint64_t prevOffset = *lastOffset;
  *lastOffset =
    ((*lastOffset / offsetAlignment) + ((*lastOffset % offsetAlignment) ? 1 : 0)) * offsetAlignment;
  return *lastOffset - prevOffset;
};
// Memory region is a memoryHeap_tgfx that a resource can be bound to
typedef struct gpuMemRegion_rt {
  std::vector<rtGpuMemBlock> memAllocations;
  uint64_t                   memoryRegionSize = 0;
  heap_tgfxhnd               memoryHeap       = {};
  void*                      mappedRegion     = {};
  uint32_t                   memoryTypeIndx   = 0;

  rtGpuMemBlock        allocateMemBlock(heapRequirementsInfo_tgfx reqs);
  static rtGpuMemBlock allocateMemBlock(std::vector<rtGpuMemBlock>& memAllocs, uint64_t regionSize,
                                        uint64_t size, uint64_t alignment);
} rtGpuMemRegion;
// Memory block is to describe a resource's memory location
// Memory blocks may contain other memory blocks, so resources can be suballocated
// For example: gpuVertexBuffer is created for mesh rendering and bind to a memory region
//  We want to suballocate each mesh's vertex buffer from this main gpuVertexBuffer memory block.
//  So each rtGpuMeshBuffer is a suballocated rtGpuMemBlock.
struct gpuMemBlock_rt {
  // If isMapped is false, uploading is automatically handled by allocating a staging buffer
  bool            isActive = false, isMapped = false, isResourceBuffer = false;
  rtGpuMemRegion* memoryRegion;
  void*           resource;
  uint64_t        offset, size;
};
rtGpuMemBlock gpuMemRegion_rt::allocateMemBlock(std::vector<rtGpuMemBlock>& memAllocs,
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
rtGpuMemBlock gpuMemRegion_rt::allocateMemBlock(heapRequirementsInfo_tgfx reqs) {
  rtGpuMemBlock memBlock =
    allocateMemBlock(memAllocations, memoryRegionSize, reqs.size, reqs.offsetAlignment);
  if (!memBlock) {
    return nullptr;
  }
  memBlock->isMapped     = mappedRegion ? true : false;
  memBlock->memoryRegion = this;
  return memBlock;
}
void* rtRenderer::getBufferMappedMemPtr(rtGpuMemBlock block) {
  if (!block->isMapped) {
    return nullptr;
  }
  return ( void* )(( uintptr_t )block->memoryRegion->mappedRegion + block->offset);
}
buffer_tgfxhnd rtRenderer::getBufferTgfxHnd(rtGpuMemBlock block) {
  return block->isResourceBuffer ? ( buffer_tgfxhnd )block->resource : nullptr;
}

typedef struct gpuResourceType_rt {
  int resourceTypeIndx;
  void (*allocateCallback)(rtGpuMemBlock allocatedRegion);
} rtGpuResourceType;
struct rtRenderer_private {
 private:
  static rtGpuMemBlock findRegionAndAllocateMemBlock(rtGpuMemRegion**          memRegion,
                                                     heapRequirementsInfo_tgfx reqs) {
    auto allocate = [reqs](rtGpuMemRegion* memRegion) -> rtGpuMemBlock {
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

    if (reqs.memoryRegionIDs[m_devLocalAllocations.memoryTypeIndx] && !block) {
      *memRegion = &m_devLocalAllocations;
      block      = allocate(*memRegion);
    }
    if (reqs.memoryRegionIDs[m_stagingAllocations.memoryTypeIndx] && !block) {
      *memRegion = &m_stagingAllocations;
      block      = allocate(*memRegion);
    }
    return block;
  }

 public:
  static rtGpuMemRegion m_devLocalAllocations, m_stagingAllocations;
  // All meshes are sub-allocations from these main buffers
  static rtGpuMemBlock                      m_gpuCustomDepthRT, m_gpuCamBuffer;
  static uint32_t                           m_activeSwpchnIndx;
  static std::vector<commandBundle_tgfxhnd> m_uploadBundles, m_renderBundles;
  static bindingTableDescription_tgfx       m_camBindingDesc;
  static bindingTable_tgfxhnd               m_camBindingTables[swapchainTextureCount];

  static rtGpuMemBlock allocateBuffer(uint64_t size, bufferUsageMask_tgfxflag usageFlag,
                                      rtGpuMemRegion* memRegion = nullptr) {
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
  static rtGpuMemBlock allocateTexture(const textureDescription_tgfx& desc,
                                       rtGpuMemRegion*                memRegion = nullptr) {
    texture_tgfxhnd tex;
    contentManager->createTexture(gpu, &desc, &tex);
    heapRequirementsInfo_tgfx reqs;
    contentManager->getHeapRequirement_Texture(tex, 0, nullptr, &reqs);

    rtGpuMemBlock memBlock     = {};
    memBlock                   = findRegionAndAllocateMemBlock(&memRegion, reqs);
    memBlock->isResourceBuffer = false;
    memBlock->resource         = tex;
    if (contentManager->bindToHeap_Texture(m_devLocalAllocations.memoryHeap, memBlock->offset, tex,
                                           0, nullptr) != result_tgfx_SUCCESS) {
      assert(0 && "Bind to Heap failed!");
    }
    return memBlock;
  }
};
rtGpuMemBlock rtRenderer_private::m_gpuCustomDepthRT = {}, rtRenderer_private::m_gpuCamBuffer = {};
gpuMemRegion_rt rtRenderer_private::m_devLocalAllocations                 = {},
                rtRenderer_private::m_stagingAllocations                  = {};
uint32_t                           rtRenderer_private::m_activeSwpchnIndx = 0;
std::vector<commandBundle_tgfxhnd> rtRenderer_private::m_uploadBundles    = {},
                                   rtRenderer_private::m_renderBundles    = {};
bindingTable_tgfxhnd         rtRenderer_private::m_camBindingTables[swapchainTextureCount];
bindingTableDescription_tgfx rtRenderer_private::m_camBindingDesc = {};
static shaderSource_tgfxhnd  fragShader                           = {};

rtGpuMemBlock rtRenderer::allocateMemoryBlock(bufferUsageMask_tgfxflag flag, uint64_t size,
                                              regionType memType) {
  rtGpuMemRegion* region = {};
  switch (memType) {
    case UPLOAD: region = &rtRenderer_private::m_stagingAllocations; break;
    case LOCAL: region = &rtRenderer_private::m_devLocalAllocations; break;
  }
  return rtRenderer_private::allocateBuffer(size, flag, region);
}

void createGPU() {
  tgfx->load_backend(nullptr, backends_tgfx_VULKAN, nullptr);

  gpu_tgfxhnd  GPUs[4]  = {};
  unsigned int gpuCount = 0;
  tgfx->getGPUlist(&gpuCount, GPUs);
  tgfx->getGPUlist(&gpuCount, GPUs);
  GPU_INDEX = glm::min(gpuCount - 1, GPU_INDEX);
  gpu       = GPUs[GPU_INDEX];
  tgfx->helpers->getGPUInfo_General(gpu, &gpuDesc);
  printf("\n\nGPU Name: %s\n  Queue Fam Count: %u\n", gpuDesc.name, gpuDesc.queueFamilyCount);
  tgfx->initGPU(gpu);
}

void createFirstWindow(tgfx_windowKeyCallback keyCB) {
  // Create window and the swapchain
  monitor_tgfxhnd mainMonitor;
  {
    monitor_tgfxhnd monitorList[16] = {};
    uint32_t        monitorCount    = 0;
    // Get monitor list
    tgfx->getMonitorList(&monitorCount, monitorList);
    tgfx->getMonitorList(&monitorCount, monitorList);
    printf("Monitor Count: %u\n", monitorCount);
    mainMonitor = monitorList[0];
  }

  // Create window (OS operation) on first monitor
  {
    tgfx_window_description windowDesc = {};
    windowDesc.size                    = {1280, 720};
    windowDesc.mode                    = windowmode_tgfx_WINDOWED;
    windowDesc.monitor                 = mainMonitor;
    windowDesc.name                    = gpuDesc.name;
    windowDesc.resizeCb                = nullptr;
    windowDesc.keyCb                   = keyCB;
    tgfx->createWindow(&windowDesc, nullptr, &mainWindowRT);
  }

  // Create swapchain (GPU operation) on the window
  {
    tgfx->helpers->getWindow_GPUSupport(mainWindowRT, gpu, &swapchainSupport);

    swpchn_desc.channels       = swapchainSupport.channels[0];
    swpchn_desc.colorSpace     = colorspace_tgfx_sRGB_NONLINEAR;
    swpchn_desc.composition    = windowcomposition_tgfx_OPAQUE;
    swpchn_desc.imageCount     = swapchainTextureCount;
    swpchn_desc.swapchainUsage = textureAllUsages;

    swpchn_desc.presentationMode = windowpresentation_tgfx_IMMEDIATE;
    swpchn_desc.window           = mainWindowRT;
    // Get all supported queues of the first GPU
    for (uint32_t i = 0; i < TGFX_WINDOWGPUSUPPORT_MAXQUEUECOUNT; i++) {
      if (!swapchainSupport.queues[i]) {
        break;
      }
      allQueues[i] = swapchainSupport.queues[i];
      swpchn_desc.permittedQueueCount++;
    }
    swpchn_desc.permittedQueues = allQueues;
    // Create swapchain
    tgfx->createSwapchain(gpu, &swpchn_desc, swpchnTextures);
  }
#ifdef NDEBUG
  printf("createGPUandFirstWindow() finished!\n");
#endif
}

void createDeviceLocalResources() {
  static constexpr uint32_t heapSize           = 1 << 27;
  uint32_t                  deviceLocalMemType = UINT32_MAX, hostVisibleMemType = UINT32_MAX;
  for (uint32_t memTypeIndx = 0; memTypeIndx < gpuDesc.memRegionsCount; memTypeIndx++) {
    const memoryDescription_tgfx& memDesc = gpuDesc.memRegions[memTypeIndx];
    if (memDesc.allocationtype == memoryallocationtype_HOSTVISIBLE ||
        memDesc.allocationtype == memoryallocationtype_FASTHOSTVISIBLE) {
      // If there 2 different memory types with same allocation type, select the bigger one!
      if (hostVisibleMemType != UINT32_MAX &&
          gpuDesc.memRegions[hostVisibleMemType].max_allocationsize > memDesc.max_allocationsize) {
        continue;
      }
      hostVisibleMemType = memTypeIndx;
    }
    if (memDesc.allocationtype == memoryallocationtype_DEVICELOCAL) {
      // If there 2 different memory types with same allocation type, select the bigger one!
      if (deviceLocalMemType != UINT32_MAX &&
          gpuDesc.memRegions[deviceLocalMemType].max_allocationsize > memDesc.max_allocationsize) {
        continue;
      }
      deviceLocalMemType = memTypeIndx;
    }
  }
  assert(hostVisibleMemType != UINT32_MAX && deviceLocalMemType != UINT32_MAX &&
         "An appropriate memory region isn't found!");
  // Create Host Visible (staging) memory heap
  contentManager->createHeap(gpu, gpuDesc.memRegions[hostVisibleMemType].memorytype_id, heapSize, 0,
                             nullptr, &rtRenderer_private::m_stagingAllocations.memoryHeap);
  contentManager->mapHeap(rtRenderer_private::m_stagingAllocations.memoryHeap, 0, heapSize, 0,
                          nullptr, &rtRenderer_private::m_stagingAllocations.mappedRegion);
  rtRenderer_private::m_stagingAllocations.memoryRegionSize = heapSize;
  rtRenderer_private::m_stagingAllocations.memoryTypeIndx   = hostVisibleMemType;
  // Create Device Logal memory heap
  contentManager->createHeap(gpu, gpuDesc.memRegions[deviceLocalMemType].memorytype_id, heapSize, 0,
                             nullptr, &rtRenderer_private::m_devLocalAllocations.memoryHeap);
  rtRenderer_private::m_devLocalAllocations.memoryTypeIndx   = deviceLocalMemType;
  rtRenderer_private::m_devLocalAllocations.memoryRegionSize = heapSize;

  textureDescription_tgfx textureDesc = {};
  textureDesc.channelType             = depthRTFormat;
  textureDesc.dataOrder               = textureOrder_tgfx_SWIZZLE;
  textureDesc.dimension               = texture_dimensions_tgfx_2D;
  textureDesc.height                  = 720;
  textureDesc.width                   = 1280;
  textureDesc.mipCount                = 1;
  textureDesc.permittedQueues         = allQueues;
  textureDesc.usage = textureUsageMask_tgfx_RENDERATTACHMENT | textureUsageMask_tgfx_COPYFROM |
                      textureUsageMask_tgfx_COPYTO;
  rtRenderer_private::m_gpuCustomDepthRT = rtRenderer_private::allocateTexture(textureDesc);

#ifdef NDEBUG
  printf("createDeviceLocalResources() finished!\n");
#endif
}

void compileShadersandPipelines() {
  // Compile compute shader, create binding table type & compute pipeline
  {
    const char* shaderText = ( const char* )filesys->funcs->read_textfile(
      string_type_tapi_CHAR_U,
      SOURCE_DIR "dependencies/TuranLibraries/shaders/firstComputeShader.comp",
      string_type_tapi_CHAR_U);
    shaderSource_tgfxhnd firstComputeShader = nullptr;
    contentManager->compileShaderSource(gpu, shaderlanguages_tgfx_GLSL,
                                        shaderStage_tgfx_COMPUTESHADER, ( void* )shaderText,
                                        strlen(shaderText), &firstComputeShader);

    // Create binding table desc
    tgfx_binding_table_description desc = {};
    desc.DescriptorType                 = shaderdescriptortype_tgfx_BUFFER;
    desc.ElementCount                   = 1;
    desc.staticSamplers                 = nullptr;
    desc.staticSamplerCount             = 0;
    desc.visibleStagesMask = shaderStage_tgfx_COMPUTESHADER | shaderStage_tgfx_VERTEXSHADER |
                             shaderStage_tgfx_FRAGMENTSHADER;

    contentManager->createComputePipeline(firstComputeShader, 1, &desc, false,
                                          &firstComputePipeline);
  }

  // Compile default fragment shader
  {
    const char* fragShaderText = ( const char* )filesys->funcs->read_textfile(
      string_type_tapi_CHAR_U, SOURCE_DIR "Content/firstShader.frag", string_type_tapi_CHAR_U);
    contentManager->compileShaderSource(gpu, shaderlanguages_tgfx_GLSL,
                                        shaderStage_tgfx_FRAGMENTSHADER, ( void* )fragShaderText,
                                        strlen(fragShaderText), &fragShader);
  }
}

void rtRenderer::initialize(tgfx_windowKeyCallback keyCB) {
  renderer       = tgfx->renderer;
  contentManager = tgfx->contentmanager;
  createGPU();
  createFirstWindow(keyCB);
  createDeviceLocalResources();
  compileShadersandPipelines();

  renderer->createFences(gpu, 1, 0u, &fence);
  gpuQueue_tgfxhnd queuesPerFam[64];
  uint32_t         queueCount = 0;
  tgfx->helpers->getGPUInfo_Queues(gpu, 0, &queueCount, queuesPerFam);
  tgfx->helpers->getGPUInfo_Queues(gpu, 0, &queueCount, queuesPerFam);

  queue             = queuesPerFam[0];
  uint64_t duration = 0;
  TURAN_PROFILE_SCOPE_MCS(profilerSys->funcs, "queueSignal", &duration);
  renderer->queueFenceSignalWait(queue, 0, nullptr, nullptr, 1, &fence, &signalValue);
  renderer->queueSubmit(queue);

  // Color Attachment Info for Begin Render Pass
  {
    colorAttachmentInfo.imageAccess = image_access_tgfx_SHADER_SAMPLEWRITE;
    colorAttachmentInfo.loadOp      = rasterpassLoad_tgfx_CLEAR;
    colorAttachmentInfo.storeOp     = rasterpassStore_tgfx_STORE;
    float cleardata[]               = {0.5, 0.5, 0.5, 1.0};
    memcpy(colorAttachmentInfo.clearValue.data, cleardata, sizeof(cleardata));

    depthAttachmentInfo.imageAccess    = image_access_tgfx_DEPTHREADWRITE_STENCILWRITE;
    depthAttachmentInfo.loadOp         = rasterpassLoad_tgfx_CLEAR;
    depthAttachmentInfo.loadStencilOp  = rasterpassLoad_tgfx_CLEAR;
    depthAttachmentInfo.storeOp        = rasterpassStore_tgfx_STORE;
    depthAttachmentInfo.storeStencilOp = rasterpassStore_tgfx_STORE;
    depthAttachmentInfo.texture =
      ( texture_tgfxhnd )rtRenderer_private::m_gpuCustomDepthRT->resource;
  }

  // Binding table creation
  {
    rtRenderer_private::m_camBindingDesc.DescriptorType     = shaderdescriptortype_tgfx_BUFFER;
    rtRenderer_private::m_camBindingDesc.ElementCount       = 1;
    rtRenderer_private::m_camBindingDesc.staticSamplerCount = 0;
    rtRenderer_private::m_camBindingDesc.visibleStagesMask  = shaderStage_tgfx_VERTEXSHADER;
    rtRenderer_private::m_camBindingDesc.isDynamic          = true;

    for (uint32_t i = 0; i < swapchainTextureCount; i++) {
      contentManager->createBindingTable(gpu, &rtRenderer_private::m_camBindingDesc,
                                         &rtRenderer_private::m_camBindingTables[i]);
    }
  }

  // Create camera shader buffer and bind it to camera binding table
  {
    rtRenderer_private::m_gpuCamBuffer = rtRenderer_private::allocateBuffer(
      sizeof(glm::mat4) * 2 * swapchainTextureCount,
      bufferUsageMask_tgfx_COPYTO | bufferUsageMask_tgfx_STORAGEBUFFER,
      &rtRenderer_private::m_stagingAllocations);
    for (uint32_t i = 0; i < swapchainTextureCount; i++) {
      uint32_t bindingIndx = 0, bufferOffset = sizeof(glm::mat4) * 2 * i,
               bufferSize = sizeof(glm::mat4) * 2;
      contentManager->setBindingTable_Buffer(
        rtRenderer_private::m_camBindingTables[i], 1, &bindingIndx,
        ( buffer_tgfxhnd* )&rtRenderer_private::m_gpuCamBuffer->resource, &bufferOffset,
        &bufferSize, 0, {});
    }
  }

  {
    uint32_t swpchnIndx = UINT32_MAX;
    tgfx->getCurrentSwapchainTextureIndex(mainWindowRT, &swpchnIndx);
    renderer->queuePresent(queue, 1, &mainWindowRT);
    renderer->queueSubmit(queue);
  }

  STOP_PROFILE_TAPI(profilerSys->funcs);
  waitValue++;
  signalValue++;
}
void rtRenderer::getSwapchainTexture() {
  uint64_t currentFenceValue = 0;
  while (currentFenceValue < signalValue - 2) {
    renderer->getFenceValue(fence, &currentFenceValue);
    printf("Waiting for fence value %u, currentFenceValue %u!\n", signalValue - 2,
           currentFenceValue);
  }
  tgfx->getCurrentSwapchainTextureIndex(mainWindowRT, &rtRenderer_private::m_activeSwpchnIndx);
}
void rtRenderer::renderFrame() {
  {
    commandBuffer_tgfxhnd uploadCmdBuffer = renderer->beginCommandBuffer(queue, 0, {});
    renderer->executeBundles(uploadCmdBuffer, rtRenderer_private::m_uploadBundles.size(),
                             rtRenderer_private::m_uploadBundles.data(), 0, {});
    renderer->endCommandBuffer(uploadCmdBuffer);
    renderer->queueExecuteCmdBuffers(queue, 1, &uploadCmdBuffer, 0, nullptr);
  }
  // Record & submit frame's scene render command buffer
  {
    commandBuffer_tgfxhnd frameCmdBuffer = renderer->beginCommandBuffer(queue, 0, nullptr);
    colorAttachmentInfo.texture          = swpchnTextures[rtRenderer_private::m_activeSwpchnIndx];
    renderer->beginRasterpass(frameCmdBuffer, 1, &colorAttachmentInfo, depthAttachmentInfo, 0,
                              nullptr);
    renderer->executeBundles(frameCmdBuffer, rtRenderer_private::m_renderBundles.size(),
                             rtRenderer_private::m_renderBundles.data(), 0, nullptr);
    renderer->endRasterpass(frameCmdBuffer, 0, nullptr);
    renderer->endCommandBuffer(frameCmdBuffer);

    renderer->queueExecuteCmdBuffers(queue, 1, &frameCmdBuffer, 0, nullptr);
    renderer->queueFenceSignalWait(queue, 1, &fence, &waitValue, 1, &fence, &signalValue);
    renderer->queueSubmit(queue);
  }
  waitValue++;
  signalValue++;
  renderer->queuePresent(queue, 1, &mainWindowRT);
  renderer->queueSubmit(queue);

  rtRenderer_private::m_uploadBundles.clear();
  rtRenderer_private::m_renderBundles.clear();
}

shaderSource_tgfxhnd rtRenderer::getDefaultFragShader() { return fragShader; }

void rtRenderer::getRTFormats(rasterPipelineDescription_tgfx* rasterPipeDesc) {
  rasterPipeDesc->colorTextureFormats[0]    = swpchn_desc.channels;
  rasterPipeDesc->depthStencilTextureFormat = depthRTFormat;
}
unsigned int rtRenderer::getFrameIndx() { return rtRenderer_private::m_activeSwpchnIndx; }
void         rtRenderer::upload(commandBundle_tgfxhnd uploadBundle) {
  rtRenderer_private::m_uploadBundles.push_back(uploadBundle);
}
void rtRenderer::render(commandBundle_tgfxhnd renderBundle) {
  rtRenderer_private::m_renderBundles.push_back(renderBundle);
}

void rtRenderer::close() {
  // Wait for all operations to end
  uint64_t fenceVal = 0;
  while (fenceVal != waitValue) {
    renderer->getFenceValue(fence, &fenceVal);
  }
  contentManager->destroyPipeline(firstComputePipeline);
  renderer->destroyFence(fence);
}

struct gpuMeshBuffer_rt {
  uint64_t gpuSideVBOffset = {}, gpuSideIBOffset = {}, indexCount = {},
           firstVertex = {}; // If indexBufferOffset == UINT64_MAX, indexCount is vertexCount
  rtGpuMemBlock copySourceBlock      = {};
  void*         copyData             = {};
  uint64_t      copyVertexBufferSize = 0, copyIndexBufferSize = 0;
};
struct camUbo {
  glm::mat4 view;
  glm::mat4 proj;
};
camUbo*          cams = {};
extern glm::mat4 getGLMMAT4(rtMat4 src);
void             rtRenderer::setActiveFrameCamProps(const rtMat4* view, const rtMat4* proj) {
  if (!cams) {
    cams = ( camUbo* )getBufferMappedMemPtr(rtRenderer_private::m_gpuCamBuffer);
  }
  cams[rtRenderer_private::m_activeSwpchnIndx].view = getGLMMAT4(*view);
  cams[rtRenderer_private::m_activeSwpchnIndx].proj = getGLMMAT4(*proj);
}

bindingTable_tgfxhnd rtRenderer::getActiveCamBindingTable() {
  return rtRenderer_private::m_camBindingTables[getFrameIndx()];
}
const bindingTableDescription_tgfx* rtRenderer::getCamBindingDesc() {
  return &rtRenderer_private::m_camBindingDesc;
}