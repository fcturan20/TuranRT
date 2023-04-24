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
uint32_t                                  camUboOffset = {};
uint64_t                                  waitValue = 0, signalValue = 1;
fence_tgfxhnd                             fence               = {};
gpuQueue_tgfxhnd                          queue               = {};
rasterpassBeginSlotInfo_tgfx              colorAttachmentInfo = {}, depthAttachmentInfo = {};
tgfx_uvec2                                windowResolution = {1280, 720};
static uint32_t                           GPU_INDEX        = 0;
static rtGpuMemBlock                      m_gpuCustomDepthRT, m_gpuCamBuffer;
static uint32_t                           m_activeSwpchnIndx;
static std::vector<commandBundle_tgfxhnd> m_uploadBundles, m_rasterBndls, m_computeBndls;
static bindingTableDescription_tgfx       m_camBindingDesc = {}, m_swpchnStorageBindingDesc = {};
static bindingTable_tgfxhnd               m_camBindingTables[swapchainTextureCount],
  m_swpchnStorageBinding[swapchainTextureCount];
static shaderSource_tgfxhnd fragShader = {};

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

void compileShadersandPipelines() {
  // Compile default fragment shader
  {
    const char* fragShaderText = ( const char* )fileSys->read_textfile(
      string_type_tapi_UTF8, SOURCE_DIR "Content/firstShader.frag", string_type_tapi_UTF8);
    contentManager->compileShaderSource(gpu, shaderlanguages_tgfx_GLSL,
                                        shaderStage_tgfx_FRAGMENTSHADER, ( void* )fragShaderText,
                                        strlen(fragShaderText), &fragShader);
  }
}

void windowResizeCallback(window_tgfxhnd windowHnd, void* userPtr, tgfx_uvec2 resolution,
                          texture_tgfxhnd* swapchainTextures);
void recreateSwapchain();
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

  recreateSwapchain();
#ifdef NDEBUG
  logSys->log(log_type_tapi_STATUS, false, L"createGPUandFirstWindow() finished");
#endif
}

struct camUbo {
  glm::mat4 worldToView;
  glm::mat4 viewToProj;
  glm::mat4 viewToWorld;
  glm::vec4 pos_fov;
  glm::vec4 dir;
};
void createBuffersAndTextures() {
  // Create camera upload buffer and bind it to camera binding table
  {
    m_gpuCamBuffer =
      allocateBuffer(sizeof(camUbo) * swapchainTextureCount,
                     bufferUsageMask_tgfx_COPYTO | bufferUsageMask_tgfx_STORAGEBUFFER,
                     getGpuMemRegion(rtRenderer::UPLOAD));
  }

#ifdef NDEBUG
  logSys->log(log_type_tapi_STATUS, false, L"createDeviceLocalResources() finished");
#endif
}

void createBindingTables() {
  // Camera binding table
  {
    m_camBindingDesc.DescriptorType     = shaderdescriptortype_tgfx_BUFFER;
    m_camBindingDesc.ElementCount       = 1;
    m_camBindingDesc.staticSamplers     = nullptr;
    m_camBindingDesc.staticSamplerCount = 0;
    m_camBindingDesc.visibleStagesMask  = shaderStage_tgfx_COMPUTESHADER |
                                         shaderStage_tgfx_VERTEXSHADER |
                                         shaderStage_tgfx_FRAGMENTSHADER;
    m_camBindingDesc.isDynamic = false;

    for (uint32_t i = 0; i < swapchainTextureCount; i++) {
      contentManager->createBindingTable(gpu, &m_camBindingDesc, &m_camBindingTables[i]);
      uint32_t bindingIndx = 0, bufferOffset = sizeof(camUbo) * i, bufferSize = sizeof(camUbo);
      contentManager->setBindingTable_Buffer(m_camBindingTables[i], 1, &bindingIndx,
                                             ( buffer_tgfxhnd* )&m_gpuCamBuffer->resource,
                                             &bufferOffset, &bufferSize, 0, {});
    }
  }
  // Swapchain texture's storage image binding table
  {
    m_swpchnStorageBindingDesc.DescriptorType     = shaderdescriptortype_tgfx_STORAGEIMAGE;
    m_swpchnStorageBindingDesc.ElementCount       = 1;
    m_swpchnStorageBindingDesc.staticSamplers     = nullptr;
    m_swpchnStorageBindingDesc.staticSamplerCount = 0;
    m_swpchnStorageBindingDesc.visibleStagesMask  = shaderStage_tgfx_COMPUTESHADER;
    m_swpchnStorageBindingDesc.isDynamic          = false;

    for (uint32_t i = 0; i < swapchainTextureCount; i++) {
      contentManager->createBindingTable(gpu, &m_swpchnStorageBindingDesc,
                                         &m_swpchnStorageBinding[i]);
    }
  }
#ifdef NDEBUG
  logSys->log(log_type_tapi_STATUS, false, L"createBindingTables() finished");
#endif
}

void rtRenderer::initialize(tgfx_windowKeyCallback keyCB) {
  renderer       = tgfx->renderer;
  contentManager = tgfx->contentmanager;
  createGPU();
  initMemRegions();
  compileShadersandPipelines();
  createBuffersAndTextures();
  createBindingTables();
  createFirstWindow(keyCB);

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
    *(( float* )depthAttachmentInfo.clearValue.data) = 1.0f;
  }

  STOP_PROFILE_TAPI(profilerSys);
  waitValue++;
  signalValue++;
}

void rtRenderer::renderFrame() {
  // Record & queue upload command buffers
  {
    commandBuffer_tgfxhnd uploadCmdBuffer = renderer->beginCommandBuffer(queue, 0, {});
    renderer->executeBundles(uploadCmdBuffer, m_uploadBundles.size(), m_uploadBundles.data(), 0,
                             {});
    renderer->endCommandBuffer(uploadCmdBuffer);
    renderer->queueExecuteCmdBuffers(queue, 1, &uploadCmdBuffer, 0, nullptr);
  }
  // Record frame's raster & compute command buffers, then queue it
  {
    // Raster
    commandBuffer_tgfxhnd rasterCB = renderer->beginCommandBuffer(queue, 0, nullptr);
    colorAttachmentInfo.texture    = swpchnTextures[m_activeSwpchnIndx];
    renderer->beginRasterpass(rasterCB, 1, &colorAttachmentInfo, depthAttachmentInfo, 0, nullptr);
    renderer->executeBundles(rasterCB, m_rasterBndls.size(), m_rasterBndls.data(), 0, nullptr);
    renderer->endRasterpass(rasterCB, 0, nullptr);
    renderer->endCommandBuffer(rasterCB);
    renderer->queueExecuteCmdBuffers(queue, 1, &rasterCB, 0, nullptr);

    // Compute
    commandBuffer_tgfxhnd computeCB = renderer->beginCommandBuffer(queue, 0, nullptr);
    renderer->executeBundles(computeCB, m_computeBndls.size(), m_computeBndls.data(), 0, nullptr);
    renderer->endCommandBuffer(computeCB);
    renderer->queueExecuteCmdBuffers(queue, 1, &computeCB, 0, nullptr);
    renderer->queueFenceSignalWait(queue, 1, &fence, &waitValue, 1, &fence, &signalValue);
    renderer->queueSubmit(queue);
  }

  waitValue++;
  signalValue++;
  renderer->queuePresent(queue, 1, &mainWindowRT);
  renderer->queueSubmit(queue);

  m_uploadBundles.clear();
  m_rasterBndls.clear();
  m_computeBndls.clear();
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

uint32_t resizeCount = 0;
void     windowResizeCallback(window_tgfxhnd windowHnd, void* userPtr, tgfx_uvec2 resolution,
                              texture_tgfxhnd* swapchainTextures) {
  windowResolution = resolution;
  recreateSwapchain();
  logSys->log(log_type_tapi_STATUS, false, L"Resized %u, %u, %u", resolution.x, resolution.y,
                  resizeCount++);
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

  // Create depth RT
  {
    if (m_gpuCustomDepthRT) {
      rtRenderer::deallocateMemoryBlock(m_gpuCustomDepthRT);
    }

    textureDescription_tgfx textureDesc = {};
    textureDesc.channelType             = depthRTFormat;
    textureDesc.dataOrder               = textureOrder_tgfx_SWIZZLE;
    textureDesc.dimension               = texture_dimensions_tgfx_2D;
    textureDesc.resolution              = windowResolution;
    textureDesc.mipCount                = 1;
    textureDesc.permittedQueues         = allQueues;
    textureDesc.usage = textureUsageMask_tgfx_RENDERATTACHMENT | textureUsageMask_tgfx_COPYFROM |
                        textureUsageMask_tgfx_COPYTO;

    m_gpuCustomDepthRT          = allocateTexture(textureDesc, getGpuMemRegion(rtRenderer::LOCAL));
    depthAttachmentInfo.texture = ( texture_tgfxhnd )m_gpuCustomDepthRT->resource;
  }

  for (uint32_t i = 0; i < swapchainTextureCount; i++) {
    uint32_t bindingIndx = 0;
    contentManager->setBindingTable_Texture(m_swpchnStorageBinding[i], 1, &bindingIndx,
                                            &swpchnTextures[i]);
  }
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
void rtRenderer::rasterize(commandBundle_tgfxhnd renderBundle) {
  m_rasterBndls.push_back(renderBundle);
}
void rtRenderer::compute(commandBundle_tgfxhnd commandBundle) {
  m_computeBndls.push_back(commandBundle);
}

void rtRenderer::close() {
  // Wait for all operations to end
  uint64_t fenceVal = 0;
  while (fenceVal != waitValue) {
    renderer->getFenceValue(fence, &fenceVal);
  }
  renderer->destroyFence(fence);
}

struct gpuMeshBuffer_rt {
  uint64_t gpuSideVBOffset = {}, gpuSideIBOffset = {}, indexCount = {},
           firstVertex = {}; // If indexBufferOffset == UINT64_MAX, indexCount is vertexCount
  rtGpuMemBlock copySourceBlock      = {};
  void*         copyData             = {};
  uint64_t      copyVertexBufferSize = 0, copyIndexBufferSize = 0;
};
camUbo*          cams = {};
extern glm::mat4 getGLMMAT4(rtMat4 src);
void             rtRenderer::setActiveFrameCamProps(const rtMat4* view, const rtMat4* proj,
                                                    const tgfx_vec3* camPos, const tgfx_vec3* camDir,
                                                    float fov) {
  if (!cams) {
    cams = ( camUbo* )getBufferMappedMemPtr(m_gpuCamBuffer);
  }
  cams[m_activeSwpchnIndx].worldToView = getGLMMAT4(*view);
  cams[m_activeSwpchnIndx].viewToProj  = getGLMMAT4(*proj);
  cams[m_activeSwpchnIndx].viewToWorld = glm::inverse(cams[m_activeSwpchnIndx].worldToView);
  cams[m_activeSwpchnIndx].pos_fov = glm::vec4(camPos->x, camPos->y, camPos->z, fov);
  cams[m_activeSwpchnIndx].dir     = glm::vec4(camDir->x, camDir->y, camDir->z, 0.0f);
}

bindingTable_tgfxhnd rtRenderer::getActiveCamBindingTable() {
  return m_camBindingTables[getFrameIndx()];
}
bindingTable_tgfxhnd rtRenderer::getSwapchainStorageBindingTable() {
  return m_swpchnStorageBinding[getFrameIndx()];
}
const bindingTableDescription_tgfx* rtRenderer::getCamBindingDesc() { return &m_camBindingDesc; }
const bindingTableDescription_tgfx* rtRenderer::getSwapchainStorageBindingDesc() {
  return &m_swpchnStorageBindingDesc;
}
tgfx_uvec2           rtRenderer::getResolution() { return windowResolution; }
tgfx_gpu_description rtRenderer::getGpuDesc() { return gpuDesc; }