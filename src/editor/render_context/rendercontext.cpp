#include <glm/glm.hpp>
#include <vector>
#include <string.h>
#include <string>

#include <filesys_tapi.h>
#include <string_tapi.h>
#include <profiler_tapi.h>
#include <logger_tapi.h>
#include <bitset_tapi.h>

#include <tgfx_forwarddeclarations.h>
#include <tgfx_structs.h>
#include <tgfx_core.h>
#include <tgfx_gpucontentmanager.h>
#include <tgfx_helper.h>
#include <tgfx_renderer.h>
#include <tgfx_structs.h>

#include "../editor_includes.h"
#include "rendercontext.h"

//////////////// VARIABLES

const tgfx_renderer*       tgfxRenderer                                   = nullptr;
const tgfx_gpuDataManager* contentManager                                 = nullptr;
struct tgfx_gpu*           gpu                                            = nullptr;
struct tgfx_gpuQueue*      allQueues[TGFX_WINDOWGPUSUPPORT_MAXQUEUECOUNT] = {};
struct tgfx_window*        mainWindowRT;
tgfx_swapchainDescription  swpchnDesc;
tgfx_windowGPUsupport      swapchainSupport                           = {};
struct tgfx_texture*       m_swapchainTextures[swapchainTextureCount] = {};
// Create device local resources
uint32_t              camUboOffset = {};
uint64_t              waitValue = 0, signalValue = 1;
struct tgfx_fence*    fence            = {};
struct tgfx_gpuQueue* queue            = {};
tgfx_uvec2            windowResolution = {1280, 720};
static uint32_t       GPU_INDEX        = 0;
static uint32_t       m_activeSwpchnIndx;
// 0: Sampler, 1: SampledTexture, 2: ImageTexture, 4: Buffer
tgfx_bindingTableDescription m_globalBindingTableDescs[4] = {};
struct tgfx_bindingTable*    m_globalBindingTables[swapchainTextureCount][4];
uint32_sc m_setBindingElementCount = 1024, m_samplerSetID = shaderdescriptortype_tgfx_SAMPLER,
          m_sampledTextureSetID = shaderdescriptortype_tgfx_SAMPLEDTEXTURE,
          m_imageTextureSetID   = shaderdescriptortype_tgfx_STORAGEIMAGE,
          m_bufferSetID         = shaderdescriptortype_tgfx_BUFFER;
// View includes camera & color texture information
uint32_sc m_swapchainBindingID = 0;

//////////////// DECLARATIONS
void createGPU();
void windowResizeCallback(struct tgfx_window* windowHnd, void* userPtr, tgfx_uvec2 resolution,
                          struct tgfx_texture** swapchainTextures);
void recreateSwapchain();
void createFirstWindow(tgfx_windowKeyCallback keyCB);
void renderFrame();
void close();
unsigned int getFrameIndx() { return m_activeSwpchnIndx; }
unsigned int allocateBinding(shaderdescriptortype_tgfx type);
void         bindGlobalBindingTables(struct tgfx_commandBundle* bundle);
void         createGlobalBindingTables();
void         getSwapchainTexture();
void bindTexture(unsigned int bindingID, unsigned char isSampled, struct tgfx_texture* texture);
void bindSampler(unsigned int bindingID, struct tgfx_sampler* sampler);
void bindBuffer(unsigned int bindingID, struct tgfx_buffer* buffer, unsigned int offset,
                unsigned int size);
struct teShader*          compileShader(const char* code);
struct tgfx_pipeline*     createComputePipeline(unsigned int            shaderCount,
                                                struct teShader* const* shaders);
struct tgfx_pipeline*     createRasterPipeline(unsigned int            vertexShaderCount,
                                               struct teShader* const* vertexShaders,
                                               unsigned int            fragmentShaderCount,
                                               struct teShader* const* fragmentShaders,
                                               textureChannels_tgfx    colorFormats[8],
                                               textureChannels_tgfx    depthFormat);
struct rtGpuMemoryRegion* createRegion(enum rtMemoryRegionType regionType, unsigned long long size,
                                       unsigned int                            reqsCount,
                                       const struct tgfx_heapRequirementsInfo* reqs);
void                      destroyRegion(struct rtGpuMemoryRegion* region);
struct rtGpuCopyInfo      allocateSize(struct rtGpuMemoryRegion*               region,
                                       const struct tgfx_heapRequirementsInfo* req);
void deallocateSize(struct rtGpuMemoryRegion* region, const struct rtGpuCopyInfo* info);

//////////////// INITIALIZE

static void initializeRenderer(tgfx_windowKeyCallback keyCB) {
  tgfxRenderer               = tgfx->renderer;
  contentManager             = tgfx->contentmanager;
  rtRenderer* r              = new rtRenderer;
  r->createComputePipeline   = createComputePipeline;
  r->createRasterPipeline    = createRasterPipeline;
  r->compileShader           = compileShader;
  r->renderFrame             = renderFrame;
  r->close                   = close;
  r->getFrameIndx            = getFrameIndx;
  r->allocateBinding         = allocateBinding;
  r->bindGlobalBindingTables = bindGlobalBindingTables;
  r->bindBuffer              = bindBuffer;
  r->bindTexture             = bindTexture;
  r->bindSampler             = bindSampler;
  r->createRegion            = createRegion;
  r->destroyRegion           = destroyRegion;
  r->allocateSize            = allocateSize;
  r->deallocateSize          = deallocateSize;
  renderer                   = r;

  createGPU();
  createGlobalBindingTables();
  createFirstWindow(keyCB);

  tgfxRenderer->createFences(gpu, 1, 0u, &fence);
  struct tgfx_gpuQueue* queuesPerFam[64];
  uint32_t              queueCount = 0;
  tgfx->helpers->getGPUInfo_Queues(gpu, 0, &queueCount, queuesPerFam);
  tgfx->helpers->getGPUInfo_Queues(gpu, 0, &queueCount, queuesPerFam);

  queue             = queuesPerFam[0];
  uint64_t duration = 0;
  TURAN_PROFILE_SCOPE_MCS(profilerSys, "queueSignal", &duration);
  tgfxRenderer->queueFenceSignalWait(queue, 0, nullptr, nullptr, 1, &fence, &signalValue);
  tgfxRenderer->queueSubmit(queue);

  STOP_PROFILE_TAPI(profilerSys);
  waitValue++;
  signalValue++;
}

void createGPU() {
  tgfx->load_backend(nullptr, backends_tgfx_VULKAN, nullptr);

  struct tgfx_gpu* GPUs[4]  = {};
  unsigned int     gpuCount = 0;
  tgfx->getGPUlist(&gpuCount, GPUs);
  tgfx->getGPUlist(&gpuCount, GPUs);
  GPU_INDEX = glm::min(gpuCount - 1, GPU_INDEX);
  gpu       = GPUs[GPU_INDEX];
  tgfx_gpuDescription gpuDesc;
  tgfx->helpers->getGPUInfo_General(gpu, &gpuDesc);
  logSys->log(log_type_tapi_STATUS, false,
              L"GPU Name: %v\n Queue Family Count: %u\n Memory Regions Count: %u\n", gpuDesc.name,
              ( uint32_t )gpuDesc.queueFamilyCount, ( uint32_t )gpuDesc.memRegionsCount);
  tgfx->initGPU(gpu);
}

tapi_bitset* m_allocatedBindings[shaderdescriptortype_tgfx_BUFFER + 1];
void         createGlobalBindingTables() {
  for (uint32_t bindingIndx = 0; bindingIndx < 4; bindingIndx++) {
    auto& desc             = m_globalBindingTableDescs[bindingIndx];
    desc.elementCount      = m_setBindingElementCount;
    desc.isDynamic         = true;
    desc.visibleStagesMask = shaderStage_tgfx_COMPUTESHADER | shaderStage_tgfx_FRAGMENTSHADER |
                             shaderStage_tgfx_VERTEXSHADER;
    desc.descriptorType = ( shaderdescriptortype_tgfx )bindingIndx;
    for (uint32_t frameIndx = 0; frameIndx < swapchainTextureCount; frameIndx++) {
      contentManager->createBindingTable(gpu, &desc,
                                         &m_globalBindingTables[frameIndx][bindingIndx]);
    }
  }

  for (uint32_t i = 0; i < length_c(m_allocatedBindings); i++) {
    m_allocatedBindings[i] = bitsetSys->createBitset((m_globalBindingTableDescs[i].elementCount / 8) + 1);
  }

  // Allocate binding 0, because it's swapchain texture
  allocateBinding(shaderdescriptortype_tgfx_STORAGEIMAGE);
#ifdef NDEBUG
  logSys->log(log_type_tapi_STATUS, false, L"createBindingTables() finished");
#endif
}

uint32_t resizeCount = 0;
void     windowResizeCallback(struct tgfx_window* windowHnd, void* userPtr, tgfx_uvec2 resolution,
                              struct tgfx_texture** swapchainTextures) {
  windowResolution = resolution;
  recreateSwapchain();
  logSys->log(log_type_tapi_STATUS, false, L"Resized %u, %u, %u", resolution.x, resolution.y,
                  resizeCount++);
}
// Bind each swapchain texture to its set's slot 0
// imageTextureSet 0 - Binding 0: SwapchainTexture0
// imageTextureSet 1 - Binding 0: SwapchainTexture1
void bindSwapchainTextures() {
  for (uint32_t frameIndx = 0; frameIndx < swapchainTextureCount; frameIndx++) {
    contentManager->setBindingTable_Texture(m_globalBindingTables[frameIndx][m_sampledTextureSetID],
                                            0, &m_swapchainBindingID,
                                            &m_swapchainTextures[frameIndx]);
  }
}
void recreateSwapchain() {
  // Create swapchain (GPU operation) on the window
  {
    tgfx->helpers->getWindow_GPUSupport(mainWindowRT, gpu, &swapchainSupport);
    swpchnDesc = {};

    swpchnDesc.channels       = swapchainSupport.channels[0];
    swpchnDesc.colorSpace     = colorspace_tgfx_sRGB_NONLINEAR;
    swpchnDesc.composition    = windowcomposition_tgfx_OPAQUE;
    swpchnDesc.imageCount     = swapchainTextureCount;
    swpchnDesc.swapchainUsage = textureUsageMask_tgfx_COPYFROM | textureUsageMask_tgfx_COPYTO |
                                textureUsageMask_tgfx_RANDOMACCESS |
                                textureUsageMask_tgfx_RASTERSAMPLE |
                                textureUsageMask_tgfx_RENDERATTACHMENT;
    swpchnDesc.presentationMode = windowpresentation_tgfx_IMMEDIATE;
    swpchnDesc.window           = mainWindowRT;
    // Get all supported queues of the first GPU
    for (uint32_t i = 0; i < TGFX_WINDOWGPUSUPPORT_MAXQUEUECOUNT; i++) {
      if (!swapchainSupport.queues[i]) {
        break;
      }
      allQueues[i] = swapchainSupport.queues[i];
      swpchnDesc.permittedQueueCount++;
    }
    swpchnDesc.permittedQueues = allQueues;
    // Create swapchain
    tgfx->createSwapchain(gpu, &swpchnDesc, m_swapchainTextures);
  }

  bindSwapchainTextures();
}
tgfx_gpuDescription gpuDesc;
void                createFirstWindow(tgfx_windowKeyCallback keyCB) {
  // Create window and the swapchain
  struct tgfx_monitor* mainMonitor;
  {
    struct tgfx_monitor* monitorList[16] = {};
    uint32_t             monitorCount    = 0;
    // Get monitor list
    tgfx->getMonitorList(&monitorCount, monitorList);
    tgfx->getMonitorList(&monitorCount, monitorList);
    logSys->log(log_type_tapi_STATUS, false, L"Monitor count: %u", monitorCount);
    mainMonitor = monitorList[0];
  }

  // Create window (OS operation) on first monitor
  {
    tgfx_windowDescription windowDesc = {};
    windowDesc.size                   = windowResolution;
    windowDesc.mode                   = windowmode_tgfx_WINDOWED;
    windowDesc.monitor                = mainMonitor;
    windowDesc.name                   = gpuDesc.name;
    windowDesc.resizeCb               = windowResizeCallback;
    windowDesc.keyCb                  = keyCB;
    tgfx->createWindow(&windowDesc, nullptr, &mainWindowRT);
  }

  recreateSwapchain();
#ifdef NDEBUG
  logSys->log(log_type_tapi_STATUS, false, L"createGPUandFirstWindow() finished");
#endif
}
unsigned int allocateBinding(shaderdescriptortype_tgfx type) {
  uint32_t i = bitsetSys->getFirstBitIndx(m_allocatedBindings[type], false);
  bitsetSys->setBit(m_allocatedBindings[type], i, true);
  return i;
}

struct teRendererInstance {
  rtRendererDescription desc;
};
std::vector<teRendererInstance*> renderers;
void                             renderFrame() {
  getSwapchainTexture();

  for (uint32_t rendererIndx = 0; rendererIndx < renderers.size(); rendererIndx++) {
    teRendererInstance* r = renderers[rendererIndx];
    r->desc.renderFrame(r->desc.data);
    tgfxRenderer->queueFenceSignalWait(queue, 1, &fence, &waitValue, 1, &fence, &signalValue);
    tgfxRenderer->queueSubmit(queue);
    waitValue++;
    signalValue++;
  }

  tgfxRenderer->queuePresent(queue, 1, &mainWindowRT);
  tgfxRenderer->queueSubmit(queue);
}

void getSwapchainTexture() {
  uint64_t currentFenceValue = 0;
  while (currentFenceValue < signalValue - 2) {
    tgfxRenderer->getFenceValue(fence, &currentFenceValue);
    logSys->log(log_type_tapi_STATUS, false, L"Waiting for fence value %lu, currentFenceValue %lu",
                signalValue - 2, currentFenceValue);
  }
  tgfx->getCurrentSwapchainTextureIndex(mainWindowRT, &m_activeSwpchnIndx);
}

//////////////// SHADER/PIPELINE COMPILATION:

struct teShader {
  shaderStage_tgfx stage   = ( shaderStage_tgfx )0;
  const char*      code    = nullptr;
  uint32_t         codeLen = 0;
};
struct teShader* compileShader(const char* code) {
  if (!code) {
    return nullptr;
  }

  teShader* s = new teShader;
  switch (code[0]) {
    case 'v': s->stage = shaderStage_tgfx_VERTEXSHADER; break;
    case 'f': s->stage = shaderStage_tgfx_FRAGMENTSHADER; break;
    case 'c': s->stage = shaderStage_tgfx_COMPUTESHADER; break;
  }
  if (s->stage != ( shaderStage_tgfx )0) {
    code = code + 1;
  }
  s->codeLen = strlen(code);
  char* c    = new char[s->codeLen + 1]{'\0'};
  memcpy(c, code, s->codeLen + 1);
  s->code = c;
  return s;
}
struct tgfx_pipeline* createComputePipeline(unsigned int            shaderCount,
                                            struct teShader* const* shaders) {
  tgfx_shaderSource* source;
  std::string        code;
  for (uint32_t i = 0; i < shaderCount; i++) {
    teShader* s = shaders[i];
    if (s->stage == shaderStage_tgfx_FRAGMENTSHADER || s->stage == shaderStage_tgfx_COMPUTESHADER) {
      logSys->log(log_type_tapi_WARNING, true, L"computeShaders list includes incompatible shader");
      return nullptr;
    }
    code += s->code;
  }

  contentManager->compileShaderSource(gpu, shaderlanguages_tgfx_GLSL, shaderStage_tgfx_VERTEXSHADER,
                                      code.c_str(), code.size(), &source);
  tgfx_pipeline* pipe;
  contentManager->createComputePipeline(source, 4, m_globalBindingTableDescs, 0, 128, &pipe);
  return pipe;
}
struct tgfx_pipeline* createRasterPipeline(unsigned int            vertexShaderCount,
                                           struct teShader* const* vertexShaders,
                                           unsigned int            fragmentShaderCount,
                                           struct teShader* const* fragmentShaders,
                                           textureChannels_tgfx    colorFormats[8],
                                           textureChannels_tgfx    depthFormat) {
  // Create vertex shader source
  tgfx_shaderSource* sources[2];
  {
    std::string code;
    for (uint32_t i = 0; i < vertexShaderCount; i++) {
      teShader* s = vertexShaders[i];
      if (s->stage == shaderStage_tgfx_FRAGMENTSHADER ||
          s->stage == shaderStage_tgfx_COMPUTESHADER) {
        logSys->log(log_type_tapi_WARNING, true, L"vertexShader list includes incompatible shader");
        return nullptr;
      }
      code += s->code;
    }

    contentManager->compileShaderSource(gpu, shaderlanguages_tgfx_GLSL,
                                        shaderStage_tgfx_VERTEXSHADER, code.c_str(), code.size(),
                                        &sources[0]);
  }
  {
    std::string code;
    for (uint32_t i = 0; i < fragmentShaderCount; i++) {
      teShader* s = fragmentShaders[i];
      if (s->stage == shaderStage_tgfx_VERTEXSHADER || s->stage == shaderStage_tgfx_COMPUTESHADER) {
        logSys->log(log_type_tapi_WARNING, true,
                    L"fragmentShader list includes incompatible shader");
        return nullptr;
      }
      code += s->code;
    }

    contentManager->compileShaderSource(gpu, shaderlanguages_tgfx_GLSL,
                                        shaderStage_tgfx_FRAGMENTSHADER, code.c_str(), code.size(),
                                        &sources[1]);
  }
  if (!sources[0] || !sources[1]) {
    return nullptr;
  }

  tgfx_pipeline*                 pipe;
  tgfx_rasterPipelineDescription desc           = {};
  desc.shaderCount                              = 2;
  desc.shaders                                  = sources;
  tgfx_rasterStateDescription stateDesc         = {};
  stateDesc.culling                             = cullmode_tgfx_BACK;
  stateDesc.depthStencilState.depthWriteEnabled = true;
  stateDesc.depthStencilState.depthTestEnabled  = true;
  stateDesc.depthStencilState.depthCompare      = compare_tgfx_LEQUAL;
  stateDesc.polygonmode                         = polygonmode_tgfx_FILL;
  stateDesc.topology                            = vertexlisttypes_tgfx_TRIANGLELIST;
  desc.mainStates                               = &stateDesc;
  memcpy(desc.colorTextureFormats, colorFormats, sizeof(colorFormats));
  desc.depthStencilTextureFormat = depthFormat;
  desc.tableCount                = 4;
  desc.tables                    = m_globalBindingTableDescs;
  desc.pushConstantSize          = 128;
  desc.pushConstantOffset        = 0;
  contentManager->createRasterPipeline(&desc, &pipe);
  return pipe;
}

//////////////// MEMORY ALLOCATION

struct rtGpuMemoryRegion {
  tapi_bitset*            m_suitableMemoryRegions;
  std::vector<tgfx_heap*> m_heaps;
  unsigned long long      m_size;
  void*                   m_mappedRegion = nullptr;
  struct memoryBlock {
    bool          isActive = true;
    rtGpuCopyInfo copyInfo;
  };
  std::vector<memoryBlock> m_memAllocs;
  // Returns the dif between offset's previous and new value
  uint64_t calculateOffset(uint64_t* lastOffset, uint64_t offsetAlignment) {
    uint64_t prevOffset = *lastOffset;
    *lastOffset = ((*lastOffset / offsetAlignment) + ((*lastOffset % offsetAlignment) ? 1 : 0)) *
                  offsetAlignment;
    return *lastOffset - prevOffset;
  };
  // If memory region is static, allocate with this fast allocator
  rtGpuCopyInfo allocateMemBlockStatic(uint64_t size, uint64_t alignment) {
    uint64_t maxOffset = 0;
    for (memoryBlock& memBlock : m_memAllocs) {
      maxOffset =
        glm::max(maxOffset, ( uint64_t )memBlock.copyInfo.heapOffset + memBlock.copyInfo.size);
      if (memBlock.isActive || memBlock.copyInfo.size < size) {
        continue;
      }

      uint64_t memBlockOffset = memBlock.copyInfo.heapOffset;
      uint64_t dif            = calculateOffset(&memBlockOffset, alignment);
      if (memBlock.copyInfo.size < dif + size) {
        continue;
      }

      return memBlock.copyInfo;
    }

    calculateOffset(&maxOffset, alignment);
    if (maxOffset + size > m_size) {
      printf("Memory region size is exceeded!");
      rtGpuCopyInfo info = {nullptr, 0, 0, nullptr};
      return info;
    }

    rtGpuMemoryRegion::memoryBlock block;
    block.isActive                = true;
    block.copyInfo.heap           = m_heaps[0];
    block.copyInfo.mappedLocation = m_mappedRegion;
    block.copyInfo.heapOffset     = maxOffset;
    block.copyInfo.size           = size;
    m_memAllocs.push_back(block);
    return block.copyInfo;
  }
  rtGpuCopyInfo allocateMemBlockDynamic(uint64_t size, uint64_t alignment){
    logSys->log(log_type_tapi_NOTCODED, true,
                L"Dynamic memory allocation isn't implement in rtGpuMemoryRegion!");
    return rtGpuCopyInfo();
  }
  rtGpuCopyInfo allocateMemBlock(uint64_t size, uint64_t alignment) {
    if (m_size != UINT64_MAX) {
      return allocateMemBlockStatic(size, alignment);
    }
    return allocateMemBlockDynamic(size, alignment);
  }
};

struct rtGpuMemoryRegion* createRegion(enum memoryallocationtype_tgfx regionType,
                                       unsigned long long size, unsigned int reqsCount,
                                       const struct tgfx_heapRequirementsInfo* reqs) {
  if (regionType > memoryallocationtype_READBACK) {
    return nullptr;
  }

  auto bitset = bitsetSys->createBitset(4);
  for (uint32_t i = 0; i < gpuDesc.memRegionsCount; i++) {
    bitsetSys->setBit(bitset, i, true);
  }

  // Check all requirements to detect suitable ones for all of them
  for (uint32_t reqIndx = 0; reqIndx < reqsCount; reqIndx++) {
    const tgfx_heapRequirementsInfo& req = reqs[reqIndx];
    for (uint32_t memoryRegionID = 0; memoryRegionID < 32; memoryRegionID++) {
      bool isFound = false;
      for (uint32_t i = 0; i < gpuDesc.memRegionsCount; i++) {
        if (req.memoryRegionIDs[i] == memoryRegionID) {
          isFound = true;
          break;
        }
      }
      bitsetSys->setBit(bitset, memoryRegionID, isFound);
    }
  }

  // Eliminate not-preferred memory regions
  for (uint32_t memRegionID = 0; memRegionID < gpuDesc.memRegionsCount; memRegionID++) {
    const tgfx_memoryDescription& memDesc = gpuDesc.memRegions[memRegionID];
    if (regionType != memDesc.allocationType) {
      bitsetSys->setBit(bitset, memRegionID, false);
    }
  }

  // Here; bitset has only desired memory region IDs
  rtGpuMemoryRegion* region       = new rtGpuMemoryRegion;
  region->m_size                  = size;
  region->m_suitableMemoryRegions = bitset;
  if (size == UINT64_MAX) {
    // Region is full dynamic
  } else {
    // Region is full static
    tgfx_heap*   heap;
    unsigned int selectedMemRegionID = UINT32_MAX;
    for (uint32_t i = 0; i < gpuDesc.memRegionsCount; i++) {
      if (!bitsetSys->getBitValue(bitset, i)) {
        continue;
      }
      if (selectedMemRegionID == UINT32_MAX) {
        selectedMemRegionID = i;
        continue;
      }
      if (gpuDesc.memRegions[i].maxAllocationSize >
          gpuDesc.memRegions[selectedMemRegionID].maxAllocationSize) {
        selectedMemRegionID = i;
      }
    }
    contentManager->createHeap(gpu, selectedMemRegionID, size, 0, nullptr, &heap);
    region->m_heaps.push_back(heap);
  }
  return region;
}
void destroyRegion(struct rtGpuMemoryRegion* region) {}

struct rtGpuCopyInfo allocateSize(struct rtGpuMemoryRegion*               region,
                                  const struct tgfx_heapRequirementsInfo* req) {
  rtGpuCopyInfo info;
  return info;
}
void deallocateSize(struct rtGpuMemoryRegion* region, const struct rtGpuCopyInfo* info) {}

void close() {
  // Wait for all operations to end
  uint64_t fenceVal = 0;
  while (fenceVal != waitValue) {
    tgfxRenderer->getFenceValue(fence, &fenceVal);
  }
  tgfxRenderer->destroyFence(fence);
}

struct gpuMeshBuffer_rt {
  uint64_t gpuSideVBOffset = {}, gpuSideIBOffset = {}, indexCount = {},
           firstVertex = {}; // If indexBufferOffset == UINT64_MAX, indexCount is vertexCount
  rtGpuCopyInfo copySourceBlock      = {};
  void*         copyData             = {};
  uint64_t      copyVertexBufferSize = 0, copyIndexBufferSize = 0;
};
extern glm::mat4 getGLMMAT4(rtMat4 src);