#include <glm/glm.hpp>
#include <vector>
#include <string.h>

#include <filesys_tapi.h>
#include <string_tapi.h>
#include <profiler_tapi.h>
#include <logger_tapi.h>

#include <tgfx_core.h>
#include <tgfx_gpucontentmanager.h>
#include <tgfx_helper.h>
#include <tgfx_renderer.h>
#include <tgfx_structs.h>

#include "../editor_includes.h"
#include "rendercontext.h"
#include "rendererAllocator.h"

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
tgfx_swapchain_description swpchnDesc;
tgfx_window_gpu_support    swapchainSupport                      = {};
textureChannels_tgfx       depthRTFormat                         = texture_channels_tgfx_D24S8;
texture_tgfxhnd            swpchnTextures[swapchainTextureCount] = {};
// Create device local resources
pipeline_tgfxhnd                          firstComputePipeline = {};
bindingTable_tgfxhnd                      bufferBindingTable   = {};
uint32_t                                  camUboOffset         = {};
uint64_t                                  waitValue = 0, signalValue = 1;
fence_tgfxhnd                             fence               = {};
gpuQueue_tgfxhnd                          queue               = {};
rasterpassBeginSlotInfo_tgfx              colorAttachmentInfo = {}, depthAttachmentInfo = {};
tgfx_uvec2                                windowResolution = {1280, 720};
static uint32_t                           GPU_INDEX        = 0;
static rtGpuMemBlock                      m_gpuCustomDepthRT, m_gpuCamBuffer;
static uint32_t                           m_activeSwpchnIndx;
static std::vector<commandBundle_tgfxhnd> m_uploadBundles, m_renderBundles;
static bindingTableDescription_tgfx       m_camBindingDesc;
static bindingTable_tgfxhnd               m_camBindingTables[swapchainTextureCount];
static shaderSource_tgfxhnd               fragShader = {};

void createGPU() {
  tgfx->load_backend(nullptr, backends_tgfx_VULKAN, nullptr);

  gpu_tgfxhnd  GPUs[4]  = {};
  unsigned int gpuCount = 0;
  tgfx->getGPUlist(&gpuCount, GPUs);
  tgfx->getGPUlist(&gpuCount, GPUs);
  GPU_INDEX = glm::min(gpuCount - 1, GPU_INDEX);
  gpu       = GPUs[GPU_INDEX];
  tgfx->helpers->getGPUInfo_General(gpu, &gpuDesc);
  logSys->log(log_type_tapi_STATUS, false,
              L"GPU Name: %v\n Queue Family Count: %u\n Memory Regions Count: %u\n", gpuDesc.name,
              ( uint32_t )gpuDesc.queueFamilyCount, ( uint32_t )gpuDesc.memRegionsCount);
  tgfx->initGPU(gpu);
}
void windowResizeCallback(window_tgfxhnd windowHnd, void* userPtr, tgfx_uvec2 resolution,
                          texture_tgfxhnd* swapchainTextures);
void createFirstWindow(tgfx_windowKeyCallback keyCB) {
  // Create window and the swapchain
  monitor_tgfxhnd mainMonitor;
  {
    monitor_tgfxhnd monitorList[16] = {};
    uint32_t        monitorCount    = 0;
    // Get monitor list
    tgfx->getMonitorList(&monitorCount, monitorList);
    tgfx->getMonitorList(&monitorCount, monitorList);
    logSys->log(log_type_tapi_STATUS, false, L"Monitor count: %u", monitorCount);
    mainMonitor = monitorList[0];
  }

  // Create window (OS operation) on first monitor
  {
    tgfx_window_description windowDesc = {};
    windowDesc.size                    = windowResolution;
    windowDesc.mode                    = windowmode_tgfx_WINDOWED;
    windowDesc.monitor                 = mainMonitor;
    windowDesc.name                    = gpuDesc.name;
    windowDesc.resizeCb                = windowResizeCallback;
    windowDesc.keyCb                   = keyCB;
    tgfx->createWindow(&windowDesc, nullptr, &mainWindowRT);
  }

  // Create swapchain (GPU operation) on the window
  {
    tgfx->helpers->getWindow_GPUSupport(mainWindowRT, gpu, &swapchainSupport);

    swpchnDesc.channels       = swapchainSupport.channels[0];
    swpchnDesc.colorSpace     = colorspace_tgfx_sRGB_NONLINEAR;
    swpchnDesc.composition    = windowcomposition_tgfx_OPAQUE;
    swpchnDesc.imageCount     = swapchainTextureCount;
    swpchnDesc.swapchainUsage = textureAllUsages;

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
    tgfx->createSwapchain(gpu, &swpchnDesc, swpchnTextures);
  }
#ifdef NDEBUG
  printf("createGPUandFirstWindow() finished!\n");
#endif
}

void createDeviceLocalResources() {
  initMemRegions();

  textureDescription_tgfx textureDesc = {};
  textureDesc.channelType             = depthRTFormat;
  textureDesc.dataOrder               = textureOrder_tgfx_SWIZZLE;
  textureDesc.dimension               = texture_dimensions_tgfx_2D;
  textureDesc.resolution              = windowResolution;
  textureDesc.mipCount                = 1;
  textureDesc.permittedQueues         = allQueues;
  textureDesc.usage = textureUsageMask_tgfx_RENDERATTACHMENT | textureUsageMask_tgfx_COPYFROM |
                      textureUsageMask_tgfx_COPYTO;
  m_gpuCustomDepthRT = allocateTexture(textureDesc);

#ifdef NDEBUG
  printf("createDeviceLocalResources() finished!\n");
#endif
}

void compileShadersandPipelines() {
  // Compile compute shader, create binding table type & compute pipeline
  {
    const char* shaderText = ( const char* )fileSys->read_textfile(
      string_type_tapi_UTF8,
      SOURCE_DIR "dependencies/TuranLibraries/shaders/firstComputeShader.comp",
      string_type_tapi_UTF8);
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
    const char* fragShaderText = ( const char* )fileSys->read_textfile(
      string_type_tapi_UTF8, SOURCE_DIR "Content/firstShader.frag", string_type_tapi_UTF8);
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
  TURAN_PROFILE_SCOPE_MCS(profilerSys, "queueSignal", &duration);
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
    depthAttachmentInfo.texture        = ( texture_tgfxhnd )m_gpuCustomDepthRT->resource;
  }

  // Binding table creation
  {
    m_camBindingDesc.DescriptorType     = shaderdescriptortype_tgfx_BUFFER;
    m_camBindingDesc.ElementCount       = 1;
    m_camBindingDesc.staticSamplerCount = 0;
    m_camBindingDesc.visibleStagesMask  = shaderStage_tgfx_VERTEXSHADER;
    m_camBindingDesc.isDynamic          = true;

    for (uint32_t i = 0; i < swapchainTextureCount; i++) {
      contentManager->createBindingTable(gpu, &m_camBindingDesc, &m_camBindingTables[i]);
    }
  }

  // Create camera shader buffer and bind it to camera binding table
  {
    m_gpuCamBuffer = allocateBuffer(
      sizeof(glm::mat4) * 2 * swapchainTextureCount,
      bufferUsageMask_tgfx_COPYTO | bufferUsageMask_tgfx_STORAGEBUFFER, getGpuMemRegion(UPLOAD));
    for (uint32_t i = 0; i < swapchainTextureCount; i++) {
      uint32_t bindingIndx = 0, bufferOffset = sizeof(glm::mat4) * 2 * i,
               bufferSize = sizeof(glm::mat4) * 2;
      contentManager->setBindingTable_Buffer(m_camBindingTables[i], 1, &bindingIndx,
                                             ( buffer_tgfxhnd* )&m_gpuCamBuffer->resource,
                                             &bufferOffset, &bufferSize, 0, {});
    }
  }

  {
    uint32_t swpchnIndx = UINT32_MAX;
    tgfx->getCurrentSwapchainTextureIndex(mainWindowRT, &swpchnIndx);
    renderer->queuePresent(queue, 1, &mainWindowRT);
    renderer->queueSubmit(queue);
  }

  STOP_PROFILE_TAPI(profilerSys);
  waitValue++;
  signalValue++;
}
void rtRenderer::getSwapchainTexture() {
  uint64_t currentFenceValue = 0;
  while (currentFenceValue < signalValue - 2) {
    renderer->getFenceValue(fence, &currentFenceValue);
    logSys->log(log_type_tapi_STATUS, false, L"Waiting for fence value %lu, currentFenceValue %lu",
                signalValue - 2, currentFenceValue);
  }
  tgfx->getCurrentSwapchainTextureIndex(mainWindowRT, &m_activeSwpchnIndx);
}
void rtRenderer::renderFrame() {
  {
    commandBuffer_tgfxhnd uploadCmdBuffer = renderer->beginCommandBuffer(queue, 0, {});
    renderer->executeBundles(uploadCmdBuffer, m_uploadBundles.size(), m_uploadBundles.data(), 0,
                             {});
    renderer->endCommandBuffer(uploadCmdBuffer);
    renderer->queueExecuteCmdBuffers(queue, 1, &uploadCmdBuffer, 0, nullptr);
  }
  // Record & submit frame's scene render command buffer
  {
    commandBuffer_tgfxhnd frameCmdBuffer = renderer->beginCommandBuffer(queue, 0, nullptr);
    colorAttachmentInfo.texture          = swpchnTextures[m_activeSwpchnIndx];
    renderer->beginRasterpass(frameCmdBuffer, 1, &colorAttachmentInfo, depthAttachmentInfo, 0,
                              nullptr);
    renderer->executeBundles(frameCmdBuffer, m_renderBundles.size(), m_renderBundles.data(), 0,
                             nullptr);
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

  m_uploadBundles.clear();
  m_renderBundles.clear();
}
void windowResizeCallback(window_tgfxhnd windowHnd, void* userPtr, tgfx_uvec2 resolution,
                          texture_tgfxhnd* swapchainTextures) {
  tgfx->createSwapchain(gpu, &swpchnDesc, swpchnTextures);
  windowResolution = resolution;
  logSys->log(log_type_tapi_STATUS, false, L"Resized %u, %u", resolution.x, resolution.y);
  rtRenderer::deallocateMemoryBlock(m_gpuCustomDepthRT);

  textureDescription_tgfx textureDesc = {};
  textureDesc.channelType             = depthRTFormat;
  textureDesc.dataOrder               = textureOrder_tgfx_SWIZZLE;
  textureDesc.dimension               = texture_dimensions_tgfx_2D;
  textureDesc.resolution              = windowResolution;
  textureDesc.mipCount                = 1;
  textureDesc.permittedQueues         = allQueues;
  textureDesc.usage = textureUsageMask_tgfx_RENDERATTACHMENT | textureUsageMask_tgfx_COPYFROM |
                      textureUsageMask_tgfx_COPYTO;
  m_gpuCustomDepthRT          = allocateTexture(textureDesc);
  depthAttachmentInfo.texture = ( texture_tgfxhnd )m_gpuCustomDepthRT->resource;
  uint32_t swpchnIndx         = UINT32_MAX;
  tgfx->getCurrentSwapchainTextureIndex(mainWindowRT, &swpchnIndx);
  renderer->queuePresent(queue, 1, &mainWindowRT);
  renderer->queueSubmit(queue);
}

shaderSource_tgfxhnd rtRenderer::getDefaultFragShader() { return fragShader; }

void rtRenderer::getRTFormats(rasterPipelineDescription_tgfx* rasterPipeDesc) {
  rasterPipeDesc->colorTextureFormats[0]    = swpchnDesc.channels;
  rasterPipeDesc->depthStencilTextureFormat = depthRTFormat;
}
unsigned int rtRenderer::getFrameIndx() { return m_activeSwpchnIndx; }
void         rtRenderer::upload(commandBundle_tgfxhnd uploadBundle) {
  m_uploadBundles.push_back(uploadBundle);
}
void rtRenderer::render(commandBundle_tgfxhnd renderBundle) {
  m_renderBundles.push_back(renderBundle);
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
    cams = ( camUbo* )getBufferMappedMemPtr(m_gpuCamBuffer);
  }
  cams[m_activeSwpchnIndx].view = getGLMMAT4(*view);
  cams[m_activeSwpchnIndx].proj = getGLMMAT4(*proj);
}

bindingTable_tgfxhnd rtRenderer::getActiveCamBindingTable() {
  return m_camBindingTables[getFrameIndx()];
}
const bindingTableDescription_tgfx* rtRenderer::getCamBindingDesc() { return &m_camBindingDesc; }
tgfx_uvec2                          rtRenderer::getResolution() { return windowResolution; }
tgfx_gpu_description                rtRenderer::getGpuDesc() { return gpuDesc; }