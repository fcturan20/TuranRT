#include <filesys_tapi.h>
#include <string.h>

#include <glm/glm.hpp>
#include <vector>

#include "../editor_includes.h"
#include "profiler_tapi.h"
#include "tgfx_core.h"
#include "tgfx_gpucontentmanager.h"
#include "tgfx_helper.h"
#include "tgfx_renderer.h"
#include "tgfx_structs.h"

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
window_tgfxhnd             window;
tgfx_swapchain_description swpchn_desc;
static constexpr uint32_t  swapchainTextureCount                 = 2;
static constexpr uint32_t  INIT_GPUINDEX                         = 0;
tgfx_window_gpu_support    swapchainSupport                      = {};
textureChannels_tgfx       depthRTFormat                         = texture_channels_tgfx_D24S8;
texture_tgfxhnd            swpchnTextures[swapchainTextureCount] = {};
// Create device local resources
bindingTableType_tgfxhnd bufferBindingType    = {};
pipeline_tgfxhnd         firstComputePipeline = {};
pipeline_tgfxhnd         firstRasterPipeline  = {};
bindingTable_tgfxhnd     bufferBindingTable   = {};
uint32_t                 camUboOffset         = {};
commandBundle_tgfxhnd    standardDrawBundle, initBundle, perSwpchnCmdBundles[swapchainTextureCount];
uint64_t                 waitValue = 0, signalValue = 1;
fence_tgfxhnd            fence                   = {};
gpuQueue_tgfxhnd         queue                   = {};
rasterpassBeginSlotInfo_tgfx colorAttachmentInfo = {}, depthAttachmentInfo = {};
window_tgfxhnd               windowlst[2]           = {};
commandBundle_tgfxhnd        standardDrawBundles[2] = {};
fence_tgfxhnd                waitFences[2]          = {};

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

    if (reqs.memoryRegionIDs[devLocalAllocations.memoryTypeIndx] && !block) {
      *memRegion = &devLocalAllocations;
      block      = allocate(*memRegion);
    }
    if (reqs.memoryRegionIDs[stagingAllocations.memoryTypeIndx] && !block) {
      *memRegion = &stagingAllocations;
      block      = allocate(*memRegion);
    }
    return block;
  }

 public:
  static rtGpuMemRegion devLocalAllocations, stagingAllocations;
  // All meshes are sub-allocations from these main buffers
  static rtGpuMemBlock                      gpuCustomDepthRT;
  static uint32_t                           activeSwpchnIndx;
  static std::vector<commandBundle_tgfxhnd> uploadBundles;

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
    contentManager->getHeapRequirement_Buffer(buf, {}, &reqs);

    rtGpuMemBlock memBlock = {};
    memBlock               = findRegionAndAllocateMemBlock(&memRegion, reqs);
    if (memBlock) {
      memBlock->isResourceBuffer = true;
      memBlock->resource         = buf;
      if (contentManager->bindToHeap_Buffer(memRegion->memoryHeap, memBlock->offset, buf, {}) !=
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
    contentManager->getHeapRequirement_Texture(tex, nullptr, &reqs);

    rtGpuMemBlock memBlock     = {};
    memBlock                   = findRegionAndAllocateMemBlock(&memRegion, reqs);
    memBlock->isResourceBuffer = false;
    memBlock->resource         = tex;
    if (contentManager->bindToHeap_Texture(devLocalAllocations.memoryHeap, memBlock->offset, tex,
                                           {}) != result_tgfx_SUCCESS) {
      assert(0 && "Bind to Heap failed!");
    }
    return memBlock;
  }
};
rtGpuMemBlock   rtRenderer_private::gpuCustomDepthRT                    = {};
gpuMemRegion_rt rtRenderer_private::devLocalAllocations                 = {},
                rtRenderer_private::stagingAllocations                  = {};
uint32_t                           rtRenderer_private::activeSwpchnIndx = 0;
std::vector<commandBundle_tgfxhnd> rtRenderer_private::uploadBundles    = {};

rtGpuMemBlock rtRenderer::allocateMemoryBlock(bufferUsageMask_tgfxflag flag, uint64_t size,
                                              regionType memType) {
  rtGpuMemRegion* region = {};
  switch (memType) {
    case UPLOAD: region = &rtRenderer_private::stagingAllocations; break;
    case LOCAL: region = &rtRenderer_private::devLocalAllocations; break;
  }
  return rtRenderer_private::allocateBuffer(size, flag, region);
}

void createGPU() {
  tgfx->load_backend(nullptr, backends_tgfx_VULKAN, nullptr);

  gpu_tgfxlsthnd gpus;
  tgfx->getGPUlist(&gpus);
  TGFXLISTCOUNT(tgfx, gpus, gpuCount);
  gpu = gpus[INIT_GPUINDEX];
  tgfx->helpers->getGPUInfo_General(gpu, &gpuDesc);
  printf("\n\nGPU Name: %s\n  Queue Fam Count: %u\n", gpuDesc.name, gpuDesc.queueFamilyCount);
  tgfx->initGPU(gpu);
}

void createFirstWindow(tgfx_windowKeyCallback keyCB) {
  // Create window and the swapchain
  monitor_tgfxlsthnd monitors;
  {
    // Get monitor list
    tgfx->getmonitorlist(&monitors);
    TGFXLISTCOUNT(tgfx, monitors, monitorCount);
    printf("Monitor Count: %u\n", monitorCount);
  }

  // Create window (OS operation) on first monitor
  {
    tgfx_window_description windowDesc = {};
    windowDesc.size                    = {1280, 720};
    windowDesc.Mode                    = windowmode_tgfx_WINDOWED;
    windowDesc.monitor                 = monitors[0];
    windowDesc.NAME                    = gpuDesc.name;
    windowDesc.ResizeCB                = nullptr;
    windowDesc.keyCB                   = keyCB;
    tgfx->createWindow(&windowDesc, nullptr, &window);
  }

  // Create swapchain (GPU operation) on the window
  {
    tgfx->helpers->getWindow_GPUSupport(window, gpu, &swapchainSupport);

    swpchn_desc.channels       = swapchainSupport.channels[0];
    swpchn_desc.colorSpace     = colorspace_tgfx_sRGB_NONLINEAR;
    swpchn_desc.composition    = windowcomposition_tgfx_OPAQUE;
    swpchn_desc.imageCount     = swapchainTextureCount;
    swpchn_desc.swapchainUsage = textureAllUsages;

    swpchn_desc.presentationMode = windowpresentation_tgfx_IMMEDIATE;
    swpchn_desc.window           = window;
    // Get all supported queues of the first GPU
    for (uint32_t i = 0; i < TGFX_WINDOWGPUSUPPORT_MAXQUEUECOUNT; i++) {
      allQueues[i] = swapchainSupport.queues[i];
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
  contentManager->createHeap(gpu, gpuDesc.memRegions[hostVisibleMemType].memorytype_id, heapSize,
                             nullptr, &rtRenderer_private::stagingAllocations.memoryHeap);
  contentManager->mapHeap(rtRenderer_private::stagingAllocations.memoryHeap, 0, heapSize, nullptr,
                          &rtRenderer_private::stagingAllocations.mappedRegion);
  rtRenderer_private::stagingAllocations.memoryRegionSize = heapSize;
  rtRenderer_private::stagingAllocations.memoryTypeIndx   = hostVisibleMemType;
  // Create Device Logal memory heap
  contentManager->createHeap(gpu, gpuDesc.memRegions[deviceLocalMemType].memorytype_id, heapSize,
                             nullptr, &rtRenderer_private::devLocalAllocations.memoryHeap);
  rtRenderer_private::devLocalAllocations.memoryTypeIndx   = deviceLocalMemType;
  rtRenderer_private::devLocalAllocations.memoryRegionSize = heapSize;

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
  rtRenderer_private::gpuCustomDepthRT = rtRenderer_private::allocateTexture(textureDesc);

#ifdef NDEBUG
  printf("createDeviceLocalResources() finished!\n");
#endif
}

void compileShadersandPipelines() {
  // Compile compute shader, create binding table type & compute pipeline
  {
    const char* shaderText = filesys->funcs->read_textfile(
      SOURCE_DIR "dependencies/TuranLibraries/shaders/firstComputeShader.comp");
    shaderSource_tgfxhnd firstComputeShader = nullptr;
    contentManager->compileShaderSource(gpu, shaderlanguages_tgfx_GLSL,
                                        shaderStage_tgfx_COMPUTESHADER, ( void* )shaderText,
                                        strlen(shaderText), &firstComputeShader);

    // Create binding table types
    {
      tgfx_binding_table_description desc = {};
      desc.DescriptorType                 = shaderdescriptortype_tgfx_BUFFER;
      desc.ElementCount                   = 1;
      desc.SttcSmplrs                     = nullptr;
      desc.visibleStagesMask = shaderStage_tgfx_COMPUTESHADER | shaderStage_tgfx_VERTEXSHADER |
                               shaderStage_tgfx_FRAGMENTSHADER;
      contentManager->createBindingTableType(gpu, &desc, &bufferBindingType);
    }

    bindingTableType_tgfxhnd bindingTypes[2] = {bufferBindingType,
                                                ( bindingTableType_tgfxhnd )tgfx->INVALIDHANDLE};
    contentManager->createComputePipeline(firstComputeShader, bindingTypes, false,
                                          &firstComputePipeline);
  }

  // Compile first vertex-fragment shader & raster pipeline
  {
    shaderSource_tgfxhnd shaderSources[2] = {};
    const char*          vertShaderText =
      filesys->funcs->read_textfile(SOURCE_DIR "Content/firstShader.vert");
    shaderSource_tgfxhnd& firstVertShader = shaderSources[0];
    contentManager->compileShaderSource(gpu, shaderlanguages_tgfx_GLSL,
                                        shaderStage_tgfx_VERTEXSHADER, ( void* )vertShaderText,
                                        strlen(vertShaderText),
                                        // vertShaderBin, vertDataSize,
                                        &firstVertShader);

    const char* fragShaderText =
      filesys->funcs->read_textfile(SOURCE_DIR "Content/firstShader.frag");
    shaderSource_tgfxhnd& firstFragShader = shaderSources[1];
    contentManager->compileShaderSource(gpu, shaderlanguages_tgfx_GLSL,
                                        shaderStage_tgfx_FRAGMENTSHADER, ( void* )fragShaderText,
                                        strlen(fragShaderText),
                                        // fragShaderBin, fragDataSize,
                                        &firstFragShader);

    vertexAttributeDescription_tgfx attribs[3];
    vertexBindingDescription_tgfx   bindings[1];
    {
      attribs[0].attributeIndx = 0;
      attribs[0].bindingIndx   = 0;
      attribs[0].dataType      = datatype_tgfx_VAR_VEC3;
      attribs[0].offset        = 0;

      attribs[1].attributeIndx = 1;
      attribs[1].bindingIndx   = 0;
      attribs[1].dataType      = datatype_tgfx_VAR_VEC3;
      attribs[1].offset        = 8;

      attribs[2].attributeIndx = 2;
      attribs[2].bindingIndx   = 0;
      attribs[2].dataType      = datatype_tgfx_VAR_VEC2;
      attribs[2].offset        = 0;

      bindings[0].bindingIndx = 0;
      bindings[0].inputRate   = vertexBindingInputRate_tgfx_VERTEX;
      bindings[0].stride      = 32;
    }

    rasterStateDescription_tgfx stateDesc         = {};
    stateDesc.culling                             = cullmode_tgfx_OFF;
    stateDesc.polygonmode                         = polygonmode_tgfx_FILL;
    stateDesc.topology                            = vertexlisttypes_tgfx_TRIANGLELIST;
    stateDesc.depthStencilState.depthTestEnabled  = true;
    stateDesc.depthStencilState.depthWriteEnabled = true;
    stateDesc.depthStencilState.depthCompare      = compare_tgfx_ALWAYS;
    stateDesc.blendStates[0].blendEnabled         = false;
    stateDesc.blendStates[0].blendComponents      = textureComponentMask_tgfx_ALL;
    stateDesc.blendStates[0].alphaMode            = blendmode_tgfx_MAX;
    stateDesc.blendStates[0].colorMode            = blendmode_tgfx_ADDITIVE;
    stateDesc.blendStates[0].dstAlphaFactor       = blendfactor_tgfx_DST_ALPHA;
    stateDesc.blendStates[0].srcAlphaFactor       = blendfactor_tgfx_SRC_ALPHA;
    stateDesc.blendStates[0].dstColorFactor       = blendfactor_tgfx_DST_COLOR;
    stateDesc.blendStates[0].srcColorFactor       = blendfactor_tgfx_SRC_COLOR;
    rasterPipelineDescription_tgfx pipelineDesc   = {};
    pipelineDesc.colorTextureFormats[0]           = swpchn_desc.channels;
    pipelineDesc.depthStencilTextureFormat        = depthRTFormat;
    pipelineDesc.mainStates                       = &stateDesc;
    pipelineDesc.shaderSourceList                 = shaderSources;
    pipelineDesc.attribLayout.attribCount         = 3;
    pipelineDesc.attribLayout.bindingCount        = 1;
    pipelineDesc.attribLayout.i_attributes        = attribs;
    pipelineDesc.attribLayout.i_bindings          = bindings;
    bindingTableType_tgfxhnd bindingTypes[2]      = {bufferBindingType,
                                                     ( bindingTableType_tgfxhnd )tgfx->INVALIDHANDLE};
    pipelineDesc.typeTables                       = bindingTypes;

    contentManager->createRasterPipeline(&pipelineDesc, nullptr, &firstRasterPipeline);
    contentManager->destroyShaderSource(shaderSources[0]);
    contentManager->destroyShaderSource(shaderSources[1]);
  }
}

void recordCommandBundles() {
  initBundle = renderer->beginCommandBundle(gpu, 3, nullptr, nullptr);
  renderer->finishCommandBundle(initBundle, nullptr);
  static constexpr uint32_t cmdCount = 9;
  // Record command bundle
  standardDrawBundle = renderer->beginCommandBundle(gpu, cmdCount, firstRasterPipeline, nullptr);
  {
    bindingTable_tgfxhnd bindingTables[2] = {bufferBindingTable,
                                             ( bindingTable_tgfxhnd )tgfx->INVALIDHANDLE};
    uint32_t             cmdKey           = 0;

    renderer->cmdSetDepthBounds(standardDrawBundle, cmdKey++, 0.0f, 1.0f);
    renderer->cmdSetViewport(standardDrawBundle, cmdKey++, {0, 0, 1280, 720, 0.0f, 1.0f});
    renderer->cmdSetScissor(standardDrawBundle, cmdKey++, {0, 0}, {1280, 720});
    renderer->cmdBindPipeline(standardDrawBundle, cmdKey++, firstRasterPipeline);
    /*
    renderer->cmdBindBindingTables(standardDrawBundle, cmdKey++, bindingTables, 0,
                                   pipelineType_tgfx_RASTER);
    uint32_t meshCount = glm::min(firstModel->meshCount, 6u);

    const std::vector<indirectOperationType_tgfx> indirectOpTypes(
      meshCount, indirectOperationType_tgfx_DRAWINDEXED);
    tgfx_indirect_argument_draw_indexed* indirectArgs =
      ( tgfx_indirect_argument_draw_indexed* )mappedRegion;
    uint64_t vertexBufferOffset = camUboOffset + sizeof(camUbo), totalVertexCount = 0,
             totalIndexCount = 0;
    for (uint32_t meshIndx = 0; meshIndx < meshCount; meshIndx++) {
      defaultMesh_rt& mesh = firstModel->meshes[meshIndx];
      uintptr_t       dst =
        ( uintptr_t )mappedRegion + vertexBufferOffset + (sizeof(tri_vertex) * totalVertexCount);
      memcpy(( void* )dst, mesh.vertices, sizeof(tri_vertex) * mesh.v_count);
      indirectArgs[meshIndx].vertexOffset = totalVertexCount;
      totalVertexCount += mesh.v_count;
    }
    for (uint32_t meshIndx = 0; meshIndx < meshCount; meshIndx++) {
      defaultMesh_rt& mesh = firstModel->meshes[meshIndx];
      uintptr_t       dst  = ( uintptr_t )mappedRegion + vertexBufferOffset +
                      (sizeof(tri_vertex) * totalVertexCount) +
                      (sizeof(uint32_t) * totalIndexCount);
      indirectArgs[meshIndx].firstIndex            = totalIndexCount;
      indirectArgs[meshIndx].firstInstance         = 0;
      indirectArgs[meshIndx].indexCountPerInstance = mesh.i_count;
      indirectArgs[meshIndx].instanceCount         = 1;
      memcpy(( void* )dst, mesh.indexbuffer, sizeof(uint32_t) * mesh.i_count);
      totalIndexCount += mesh.i_count;
    }
    for (uint32_t i = 0; i < meshCount; i++) {
      defaultMesh_rt& mesh = firstModel->meshes[i];
      printf("MeshInfo: vCount = %u, iCount = %u\nIndirectArgument: iCount = %u, iOffset = %u\n\n",
             mesh.v_count, mesh.i_count, indirectArgs[i].indexCountPerInstance,
             indirectArgs[i].firstIndex);
    }
    renderer->cmdBindVertexBuffers(standardDrawBundle, cmdKey++, 0, 1, &firstBuffer,
                                   &vertexBufferOffset);
    renderer->cmdBindIndexBuffer(standardDrawBundle, cmdKey++, firstBuffer,
                                 vertexBufferOffset + (sizeof(tri_vertex) * totalVertexCount), 4);
    renderer->cmdExecuteIndirect(standardDrawBundle, cmdKey++, indirectOpTypes.size(),
                                 indirectOpTypes.data(), firstBuffer, 0, nullptr);
                                 */
    assert(cmdKey <= cmdCount && "Cmd count doesn't match!");
  }
  renderer->finishCommandBundle(standardDrawBundle, nullptr);
}

void recordSwpchnCmdBundles() {
  for (uint32_t i = 0; i < swapchainTextureCount; i++) {
    static constexpr uint32_t cmdCount = 5;
    // Record command bundle
    perSwpchnCmdBundles[i] =
      renderer->beginCommandBundle(gpu, cmdCount, firstRasterPipeline, nullptr);
    {
      bindingTable_tgfxhnd bindingTables[2] = {bufferBindingTable,
                                               ( bindingTable_tgfxhnd )tgfx->INVALIDHANDLE};
      uint32_t             cmdKey           = 0;

      renderer->cmdBindPipeline(perSwpchnCmdBundles[i], cmdKey++, firstRasterPipeline);
      renderer->cmdBindBindingTables(perSwpchnCmdBundles[i], cmdKey++, bindingTables, 0,
                                     pipelineType_tgfx_COMPUTE);
      renderer->cmdSetViewport(perSwpchnCmdBundles[i], cmdKey++, {0, 0, 1280, 720, 0.0f, 1.0f});
      renderer->cmdSetScissor(perSwpchnCmdBundles[i], cmdKey++, {0, 0}, {1280, 720});
      renderer->cmdDrawNonIndexedDirect(perSwpchnCmdBundles[i], cmdKey++, 3, 1, 0, 0);

      assert(cmdKey <= cmdCount && "Cmd count doesn't match!");
    }
    renderer->finishCommandBundle(perSwpchnCmdBundles[i], nullptr);
  }
}

void rtRenderer::initialize(tgfx_windowKeyCallback keyCB) {
  renderer       = tgfx->renderer;
  contentManager = tgfx->contentmanager;
  createGPU();
  createFirstWindow(keyCB);
  createDeviceLocalResources();
  compileShadersandPipelines();
  recordCommandBundles();

  renderer->createFences(gpu, 1, 0u, &fence);
  gpuQueue_tgfxlsthnd queuesPerFam;
  tgfx->helpers->getGPUInfo_Queues(gpu, 0, &queuesPerFam);

  queue             = queuesPerFam[0];
  uint64_t duration = 0;
  TURAN_PROFILE_SCOPE_MCS(profilerSys->funcs, "queueSignal", &duration);
  waitFences[0] = fence;
  waitFences[1] = ( fence_tgfxhnd )tgfx->INVALIDHANDLE;
  renderer->queueFenceSignalWait(queue, {}, &waitValue, waitFences, &signalValue);
  renderer->queueSubmit(queue);

  // Color Attachment Info for Begin Render Pass
  standardDrawBundles[0] = standardDrawBundle;
  standardDrawBundles[1] = ( commandBundle_tgfxhnd )tgfx->INVALIDHANDLE;
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
    depthAttachmentInfo.texture = ( texture_tgfxhnd )rtRenderer_private::gpuCustomDepthRT->resource;
  }

  // Initialization Command Buffer Recording
  commandBuffer_tgfxhnd initCmdBuffer = {};
  {
    initCmdBuffer                        = renderer->beginCommandBuffer(queue, nullptr);
    commandBundle_tgfxhnd initBundles[2] = {initBundle,
                                            ( commandBundle_tgfxhnd )tgfx->INVALIDHANDLE};
    renderer->executeBundles(initCmdBuffer, initBundles, nullptr);
    renderer->endCommandBuffer(initCmdBuffer);
  }

  // Submit initialization operations to GPU!
  windowlst[0] = window;
  windowlst[1] = ( window_tgfxhnd )tgfx->INVALIDHANDLE;
  {
    uint32_t swpchnIndx = UINT32_MAX;
    tgfx->getCurrentSwapchainTextureIndex(window, &swpchnIndx);
    if (initCmdBuffer) {
      commandBuffer_tgfxhnd initCmdBuffers[2] = {initCmdBuffer,
                                                 ( commandBuffer_tgfxhnd )tgfx->INVALIDHANDLE};
      renderer->queueExecuteCmdBuffers(queue, initCmdBuffers, nullptr);
      renderer->queueSubmit(queue);
    }
    renderer->queuePresent(queue, windowlst);
    renderer->queueSubmit(queue);
  }

  STOP_PROFILE_PRINTFUL_TAPI(profilerSys->funcs);
  waitValue++;
  signalValue++;
}
void rtRenderer::renderFrame() {
  uint64_t currentFenceValue = 0;
  while (currentFenceValue < signalValue - 2) {
    renderer->getFenceValue(fence, &currentFenceValue);
    printf("Waiting for fence value %u, currentFenceValue %u!\n", signalValue - 2,
           currentFenceValue);
  }
  tgfx->getCurrentSwapchainTextureIndex(window, &rtRenderer_private::activeSwpchnIndx);

  {
    commandBuffer_tgfxhnd uploadCmdBuffer = renderer->beginCommandBuffer(queue, {});
    rtRenderer_private::uploadBundles.push_back(( commandBundle_tgfxhnd )tgfx->INVALIDHANDLE);
    renderer->executeBundles(uploadCmdBuffer, rtRenderer_private::uploadBundles.data(), {});
    renderer->endCommandBuffer(uploadCmdBuffer);
    commandBuffer_tgfxhnd frameCmdBuffers[2] = {uploadCmdBuffer,
                                                ( commandBuffer_tgfxhnd )tgfx->INVALIDHANDLE};
    renderer->queueExecuteCmdBuffers(queue, frameCmdBuffers, nullptr);
  }
  // Record & submit frame's scene render command buffer
  {
    commandBundle_tgfxhnd frameCmdBundles[2] = {
      perSwpchnCmdBundles[rtRenderer_private::activeSwpchnIndx],
      ( commandBundle_tgfxhnd )tgfx->INVALIDHANDLE};
    commandBuffer_tgfxhnd frameCmdBuffer = renderer->beginCommandBuffer(queue, nullptr);
    colorAttachmentInfo.texture          = swpchnTextures[rtRenderer_private::activeSwpchnIndx];
    renderer->beginRasterpass(frameCmdBuffer, 1, &colorAttachmentInfo, depthAttachmentInfo, {});
    renderer->executeBundles(frameCmdBuffer, standardDrawBundles, nullptr);
    renderer->endRasterpass(frameCmdBuffer, {});
    renderer->endCommandBuffer(frameCmdBuffer);

    commandBuffer_tgfxhnd frameCmdBuffers[2] = {frameCmdBuffer,
                                                ( commandBuffer_tgfxhnd )tgfx->INVALIDHANDLE};
    renderer->queueExecuteCmdBuffers(queue, frameCmdBuffers, nullptr);
    renderer->queueFenceSignalWait(queue, {}, &waitValue, waitFences, &signalValue);
    renderer->queueSubmit(queue);
  }
  waitValue++;
  signalValue++;
  renderer->queuePresent(queue, windowlst);
  renderer->queueSubmit(queue);
}
void rtRenderer::upload(commandBundle_tgfxhnd uploadBundle) {
  rtRenderer_private::uploadBundles.push_back(uploadBundle);
}

void rtRenderer::close() {
  // Wait for all operations to end
  uint64_t fenceVal = 0;
  while (fenceVal != waitValue) {
    renderer->getFenceValue(fence, &fenceVal);
  }
  contentManager->destroyPipeline(firstRasterPipeline);
  contentManager->destroyPipeline(firstComputePipeline);
  contentManager->destroyBindingTable(bufferBindingTable);
  contentManager->destroyBindingTableType(bufferBindingType);
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
camUbo* cams = {};

void rtRenderer::setActiveFrameCamProps(glm::mat4 view, glm::mat4 proj) {
  /*
  if (!cams) {
    cams = ( camUbo* )( uintptr_t )rtRenderer_private::stagingAllocations.mappedRegion +
           rtRenderer_private::gpuCamBuffer->offset;
  }
  cams[rtRenderer_private::activeSwpchnIndx].view = view;
  cams[rtRenderer_private::activeSwpchnIndx].proj = proj;
  */
}
