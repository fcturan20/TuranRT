#include <vector>
#include <glm/glm.hpp>

#include <string_tapi.h>
#include <filesys_tapi.h>
#include <logger_tapi.h>
#include <tgfx_core.h>
#include <tgfx_forwarddeclarations.h>
#include <tgfx_renderer.h>
#include <tgfx_gpucontentmanager.h>
#include <tgfx_structs.h>

#include "../resourceSys/resourceManager.h"
#include "../editor_includes.h"
#include "../resourceSys/mesh.h"
#include "../resourceSys/shaderEffect.h"
#include "../resourceSys/surfaceMaterial.h"
#include "forwardMesh.h"
#include "../render_context/rendercontext.h"

char_sc                         attribNames[]            = {"POSITION", "TEXCOORD0", "NORMAL"};
uint32_sc                      attribCount              = length_c(attribNames);
uint32_sc                      attribElementSize[]      = {3 * 4, 2 * 4, 3 * 4};
static constexpr datatype_tgfx attribElementTypesTgfx[] = {
  datatype_tgfx_VAR_VEC3, datatype_tgfx_VAR_VEC2, datatype_tgfx_VAR_VEC3};
static_assert(length_c(attribElementSize) == attribCount &&
                length_c(attribElementTypesTgfx) == attribCount,
              "Attribute lists isn't matching");

// Maximum number of seperate meshes
// It's important because each mesh will operate a instanced indirect draw call
static constexpr uint32_t RT_MAXDEFAULTMESH_MESHCOUNT = 1024, RT_MAXTRANSFORMBUFFER_SIZE = 4 << 20;

rtGpuMemBlock *gpuVertexBuffer = {}, *gpuIndexBuffer = {}, *gpuDrawIndirectBuffer = {},
              *gpuComputeIndirectBuffer = {}, *transformsBuffer[swapchainTextureCount] = {};
std::vector<struct tgfx_commandBundle*> uploadBundleList   = {};
struct tgfx_pipeline *                  meshRasterPipeline = {}, *meshComputePipeline = {};
tgfx_bindingTableDescription            transformBindingDesc = {}, indirectDrawBindingTableDesc;
struct tgfx_bindingTable *              transformBindingTables[swapchainTextureCount] = {},
                         *indirectDrawBindingTable                                    = {},
                         *vertexBufferBindingTables[swapchainTextureCount]            = {},
                         *indexBufferBindingTables[swapchainTextureCount]             = {};
indirectOperationType_tgfx rasterOperationList[RT_MAXDEFAULTMESH_MESHCOUNT]           = {},
                           rtOperationList[RT_MAXDEFAULTMESH_MESHCOUNT]               = {};
std::vector<struct tgfx_commandBundle*> m_bundles[swapchainTextureCount];
const rtMeshManagerType*                Mmt = {};

struct rtForwardMesh {
  uint32_t m_gpuVertexOffset = 0, m_gpuIndexOffset = 0;
  bool     isAlive = false;
  // Optional parameters
  // Because mesh data will be in GPU-side as default
  // If user wants to access buffer, GPU->CPU copy is necessary
  void*          m_data;
  uint32_t       m_vertexCount, m_indexCount;
  rtGpuMemBlock* m_stagingBlock;
};
std::vector<rtForwardMesh*> meshes = {}, uploadQueue = {};

const rtMeshManagerType* forwardMM_managerType() { return Mmt; }
rtMesh* forwardMM_allocateMesh(uint32_t vertexCount, uint32_t indexCount, void** meshData) {
  uint32_t       maxVertexOffset = 0, maxIndexOffset = 0;
  rtForwardMesh* mesh = {};
  for (rtForwardMesh* searchMesh : meshes) {
    maxVertexOffset =
      glm::max(maxVertexOffset, searchMesh->m_vertexCount + searchMesh->m_gpuVertexOffset);
    maxIndexOffset =
      glm::max(maxIndexOffset, searchMesh->m_indexCount + searchMesh->m_gpuIndexOffset);
    if (searchMesh->isAlive || searchMesh->m_vertexCount < vertexCount ||
        searchMesh->m_indexCount < indexCount) {
      continue;
    }
    mesh = searchMesh;
  }
  if (!mesh) {
    mesh                    = ( rtForwardMesh* )MM_createMeshHandle(Mmt);
    mesh->m_gpuVertexOffset = maxVertexOffset;
    mesh->m_gpuIndexOffset  = maxIndexOffset;
    meshes.push_back(mesh);
  }
  mesh->isAlive       = true;
  mesh->m_vertexCount = vertexCount;
  mesh->m_indexCount  = indexCount;

  uint32_t stagingBufferSize =
    (mesh->m_vertexCount * sizeof(rtForwardVertex)) + (mesh->m_indexCount * sizeof(uint32_t));
  mesh->m_stagingBlock = rtRenderer::allocateMemoryBlock(bufferUsageMask_tgfx_COPYFROM,
                                                         stagingBufferSize, rtMemoryRegionType_UPLOAD);

  // Can't use staging memory, keep it in RAM to upload later
  if (mesh->m_stagingBlock) {
    mesh->m_data = rtRenderer::getBufferMappedMemPtr(mesh->m_stagingBlock);
  } else {
    mesh->m_data = malloc(stagingBufferSize);
  }
  *meshData = mesh->m_data;

  return ( rtMesh* )mesh;
}

// Uploads memory block of allocateMesh's meshData to GPU and validates rtMesh
// All meshData should be ready
unsigned char forwardMesh_uploadFnc(rtMesh* i_mesh) {
  rtForwardMesh* mesh = ( rtForwardMesh* )i_mesh;
  // If there is no space in upload memory, push to queue to upload later
  if (!mesh->m_stagingBlock) {
    uploadQueue.push_back(mesh);
    return false;
  }

  struct tgfx_commandBundle* copyBundle = tgfxRenderer->beginCommandBundle(gpu, 2, nullptr, 0, nullptr);
  tgfxRenderer->cmdCopyBufferToBuffer(copyBundle, 0, mesh->m_vertexCount * sizeof(rtForwardVertex),
                                  rtRenderer::getBufferTgfxHnd(mesh->m_stagingBlock), 0,
                                  rtRenderer::getBufferTgfxHnd(gpuVertexBuffer),
                                  mesh->m_gpuVertexOffset * sizeof(rtForwardVertex));
  tgfxRenderer->cmdCopyBufferToBuffer(copyBundle, 1, mesh->m_indexCount * sizeof(uint32_t),
                                  rtRenderer::getBufferTgfxHnd(mesh->m_stagingBlock),
                                  mesh->m_vertexCount * sizeof(rtForwardVertex),
                                  rtRenderer::getBufferTgfxHnd(gpuIndexBuffer),
                                  mesh->m_gpuIndexOffset * sizeof(uint32_t));
  tgfxRenderer->finishCommandBundle(copyBundle, 0, nullptr);
  uploadBundleList.push_back(copyBundle);
  return true;
}
unsigned char forwardMesh_destroyFnc(rtMesh* i_mesh) { return true; }

struct renderList {
  rtForwardMesh*       m_mesh;
  std::vector<rtMat4> m_transforms;
};
struct tgfx_commandBundle* forwardMesh_renderFnc(unsigned int               count,
                                                 const MM_renderInfo* const infos) {
  uint32_t               frameIndx = rtRenderer::getFrameIndx();
  struct rtShaderEffect* se        = SEM_getSE(infos[0].sei);
  struct tgfx_pipeline*  pipe      = SSEM_getPipeline(( struct rtSurfaceShaderEffect* )se, Mmt);

  if (m_bundles[frameIndx].size()) {
    for (struct tgfx_commandBundle* bndle : m_bundles[frameIndx]) {
      tgfxRenderer->destroyCommandBundle(bndle);
    }
  }

  m_bundles[frameIndx] = uploadBundleList;
  uploadBundleList.clear();

  // Create render list to fill binding tables
  std::vector<renderList> renders;
  for (uint32_t infoIndx = 0; infoIndx < count; infoIndx++) {
    bool isFound = false;
    for (renderList& r : renders) {
      if (r.m_mesh == ( rtForwardMesh* )infos[infoIndx].mesh) {
        for (uint32_t instanceIndx = 0; instanceIndx < infos[infoIndx].transformCount;
             instanceIndx++) {
          r.m_transforms.push_back(infos[infoIndx].transforms[instanceIndx]);
          isFound = true;
        }
        if (isFound) {
          break;
        }
      }
    }
    if (!isFound) {
      renderList r = {};
      r.m_mesh     = ( rtForwardMesh* )infos[infoIndx].mesh;
      r.m_transforms.push_back(*infos[infoIndx].transforms[instanceIndx]);
      renders.push_back(r);
    }
  }

  struct bindingTableUpdate {
    // Batched set infos
    struct tgfx_buffer* batchBuffers[RT_MAXDEFAULTMESH_MESHCOUNT] = {};
    uint32_t            batchIndices[RT_MAXDEFAULTMESH_MESHCOUNT] = {},
             batchOffsets[RT_MAXDEFAULTMESH_MESHCOUNT]            = {},
             batchSizes[RT_MAXDEFAULTMESH_MESHCOUNT]              = {};
  };
  // Fill transform & draw indirect buffer & binding table
  {
    uint32_t            transformIndx = 0;
    struct tgfx_buffer* transformsBufferTgfx =
      rtRenderer::getBufferTgfxHnd(transformsBuffer[frameIndx]);
    glm::mat4* transformsBufferMapped =
      ( glm::mat4* )rtRenderer::getBufferMappedMemPtr(transformsBuffer[frameIndx]);
    tgfx_drawIndexedIndirectArgument* drawIndirectBufferMapped =
      ( tgfx_drawIndexedIndirectArgument* )rtRenderer::getBufferMappedMemPtr(gpuDrawIndirectBuffer);

    bindingTableUpdate btUpdate;

    for (uint32_t rIndx = 0; rIndx < renders.size(); rIndx++) {
      tgfx_drawIndexedIndirectArgument lastIndirectBuffer = {};
      const renderList&                r                  = renders[rIndx];

      btUpdate.batchIndices[rIndx] = rIndx;
      btUpdate.batchBuffers[rIndx] = transformsBufferTgfx;
      btUpdate.batchOffsets[rIndx] = transformIndx * sizeof(glm::mat4);
      btUpdate.batchSizes[rIndx]   = r.m_transforms.size() * sizeof(glm::mat4);

      for (rtMat4 m : r.m_transforms) {
        transformsBufferMapped[transformIndx++] = *( glm::mat4* )&m;
      }
      lastIndirectBuffer.firstIndex            = r.m_mesh->m_gpuIndexOffset;
      lastIndirectBuffer.indexCountPerInstance = r.m_mesh->m_indexCount;
      lastIndirectBuffer.instanceCount         = r.m_transforms.size();
      lastIndirectBuffer.firstInstance         = 0;
      lastIndirectBuffer.vertexOffset          = r.m_mesh->m_gpuVertexOffset;
      drawIndirectBufferMapped[rIndx]          = lastIndirectBuffer;
    }
    for (uint32_t i = renders.size(); i < RT_MAXDEFAULTMESH_MESHCOUNT; i++) {
      btUpdate.batchIndices[i] = i;
      btUpdate.batchBuffers[i] = transformsBufferTgfx;
      btUpdate.batchOffsets[i] = 0;
      btUpdate.batchSizes[i]   = 1;
    }
    contentManager->setBindingTable_Buffer(
      transformBindingTables[frameIndx], RT_MAXDEFAULTMESH_MESHCOUNT, btUpdate.batchIndices,
      btUpdate.batchBuffers, btUpdate.batchOffsets, btUpdate.batchSizes, 0, nullptr);
  }
  // Update vertex&index buffer binding tables
  {
    bindingTableUpdate  vbUpdate, ibUpdate;
    struct tgfx_buffer *vbTgfx = rtRenderer::getBufferTgfxHnd(gpuVertexBuffer),
                       *ibTgfx = rtRenderer::getBufferTgfxHnd(gpuIndexBuffer);

    for (uint32_t rIndx = 0; rIndx < renders.size(); rIndx++) {
      const renderList& r = renders[rIndx];

      vbUpdate.batchIndices[rIndx] = rIndx;
      vbUpdate.batchBuffers[rIndx] = vbTgfx;
      vbUpdate.batchOffsets[rIndx] = r.m_mesh->m_gpuVertexOffset * sizeof(forwardVertex_rt);
      vbUpdate.batchSizes[rIndx]   = r.m_mesh->m_vertexCount * sizeof(forwardVertex_rt);

      ibUpdate.batchIndices[rIndx] = rIndx;
      ibUpdate.batchBuffers[rIndx] = ibTgfx;
      ibUpdate.batchOffsets[rIndx] = r.m_mesh->m_gpuIndexOffset * sizeof(uint32_t);
      ibUpdate.batchSizes[rIndx]   = r.m_mesh->m_indexCount * sizeof(uint32_t);
    }
    for (uint32_t i = renders.size(); i < RT_MAXDEFAULTMESH_MESHCOUNT; i++) {
      vbUpdate.batchIndices[i] = i;
      vbUpdate.batchBuffers[i] = rtRenderer::getBufferTgfxHnd(gpuVertexBuffer);
      vbUpdate.batchOffsets[i] = 0;
      vbUpdate.batchSizes[i]   = 1;

      ibUpdate.batchIndices[i] = i;
      ibUpdate.batchBuffers[i] = rtRenderer::getBufferTgfxHnd(gpuIndexBuffer);
      ibUpdate.batchOffsets[i] = 0;
      ibUpdate.batchSizes[i]   = 1;
    }
    contentManager->setBindingTable_Buffer(
      vertexBufferBindingTables[frameIndx], RT_MAXDEFAULTMESH_MESHCOUNT, vbUpdate.batchIndices,
      vbUpdate.batchBuffers, vbUpdate.batchOffsets, vbUpdate.batchSizes, 0, nullptr);
    contentManager->setBindingTable_Buffer(
      indexBufferBindingTables[frameIndx], RT_MAXDEFAULTMESH_MESHCOUNT, ibUpdate.batchIndices,
      ibUpdate.batchBuffers, ibUpdate.batchOffsets, ibUpdate.batchSizes, 0, nullptr);
  }

  // 2 binding tables (same call), 1 vertex buffer, 1 index buffer & 1 indirect
  // indexed draw call
  static constexpr uint32_t cmdCount = 7;

  struct tgfx_commandBundle* bundle =
    tgfxRenderer->beginCommandBundle(gpu, cmdCount, meshRasterPipeline, 0, nullptr);
  uint32_t                  cmdIndx          = 0;
  struct tgfx_bindingTable* bindingTables[2] = {rtRenderer::getActiveCamBindingTable(),
                                                transformBindingTables[frameIndx]};
  tgfxRenderer->cmdSetDepthBounds(bundle, cmdIndx++, 0.0f, 1.0f);
  tgfxRenderer->cmdSetScissor(bundle, cmdIndx++, {0, 0}, {1280, 720});
  tgfx_viewportInfo viewport;
  viewport.depthMinMax   = {0.0f, 1.0f};
  viewport.size          = {1280, 720};
  viewport.topLeftCorner = {0, 0};
  tgfxRenderer->cmdSetViewport(bundle, cmdIndx++, viewport);
  tgfxRenderer->cmdBindBindingTables(bundle, cmdIndx++, 0, 2, bindingTables, pipelineType_tgfx_RASTER);
  uint64_t            offset         = 0;
  struct tgfx_buffer *vertBuffer     = rtRenderer::getBufferTgfxHnd(gpuVertexBuffer),
                     *indxBuffer     = rtRenderer::getBufferTgfxHnd(gpuIndexBuffer),
                     *indirectBuffer = rtRenderer::getBufferTgfxHnd(gpuDrawIndirectBuffer);
  tgfxRenderer->cmdBindVertexBuffers(bundle, cmdIndx++, 0, 1, &vertBuffer, &offset);
  tgfxRenderer->cmdBindIndexBuffer(bundle, cmdIndx++, indxBuffer, 0, 4);
  tgfxRenderer->cmdExecuteIndirect(bundle, cmdIndx++, renders.size(), rasterOperationList,
                               indirectBuffer, 0, 0, nullptr);
  assert(cmdIndx <= cmdCount && "Cmd count is exceeded!");
  tgfxRenderer->finishCommandBundle(bundle, 0, nullptr);
  m_bundles[frameIndx].push_back(bundle);
  return bundle;
}
void forwardMesh_frameFnc() {
  for (struct tgfx_commandBundle* bndl : uploadBundleList) {
    rtRenderer::upload(bndl);
  }

  // Clear staging memory uploads, then copy queued meshes to staging memory
}

rtMesh* forwardMesh_allocateFnc(uint32_t vertexCount, uint32_t indexCount, void* extraInfo,
                                void** meshData) {
  return forwardMM_allocateMesh(vertexCount, indexCount, meshData);
}

static constexpr char
  fragCode_load_TEXTCOORD0[] =
    "layout(location = 0) in vec2 textCoord; vec2 load_TEXTCOORD0(){return textCoord;}",
  fragCode_load_NORMAL[] =
    "layout(location = 1) in vec3 vertNormal; vec3 load_NORMAL(){return vertNormal;}",
  fragCode_MAIN[] =
    "layout(location = 0) out vec4 outColor; void main() {outColor = surface_shading();}";
static constexpr wchar_t* notSupportedSE =
  L"Surface SE isn't supported by the forward mesh manager";
inline struct tgfx_shaderSource* compileFragmentShader(const char* shadingCode) {
  char* fragCode = nullptr;
  stringSys->createString(string_type_tapi_UTF8, ( void** )&fragCode,
                          L"#version 450\n%s\n%s\n%s\n%s", fragCode_load_TEXTCOORD0,
                          fragCode_load_NORMAL, shadingCode, fragCode_MAIN);
  struct tgfx_shaderSource* fragmentShader = nullptr;
  contentManager->compileShaderSource(gpu, shaderlanguages_tgfx_GLSL,
                                      shaderStage_tgfx_FRAGMENTSHADER, fragCode, strlen(fragCode),
                                      &fragmentShader);
  free(fragCode);
  return fragmentShader;
}

tgfx_rasterStateDescription     CONST_stateDesc = {};
tgfx_vertexAttributeDescription CONST_attribs[attribCount];
tgfx_vertexBindingDescription   CONST_bindings[1];
struct tgfx_shaderSource*       firstVertShader;
unsigned char                   forwardMesh_supportsSEfnc(struct rtSurfaceShaderEffect* se) {
#define earlyExit()                                    \
  logSys->log(log_type_tapi_ERROR, 0, notSupportedSE); \
  return 0

  const auto& props = SSEM_getSurfaceSEProps(se);
  if (props.attribCount > attribCount || props.language != shaderlanguages_tgfx_GLSL ||
      props.WPO_code) {
    earlyExit();
  }
  for (uint32_t i = 0; i < props.attribCount; i++) {
    const auto& attribName     = props.attribNames[i];
    const auto& attribDataType = props.dataTypes[i];
    bool        isFound        = false;
    for (uint32_t s = 0; s < attribCount && !isFound; s++) {
      if (attribDataType == attribElementTypesTgfx[s] && strcmp(attribName, attribNames[s]) == 0 &&
          props.inputRates[s] == vertexBindingInputRate_tgfx_VERTEX) {
        isFound = true;
      }
    }
    if (!isFound) {
      earlyExit();
    }
  }

  struct tgfx_shaderSource* fragmentShader = compileFragmentShader(props.shading_code);
  if (!fragmentShader) {
    earlyExit();
  }

  struct tgfx_pipeline* pipe = nullptr;
  {
    tgfx_rasterPipelineDescription pipeDesc = {};
    pipeDesc.attribLayout.attribCount       = attribCount;
    pipeDesc.attribLayout.bindingCount      = 1;
    pipeDesc.attribLayout.i_attributes      = CONST_attribs;
    pipeDesc.attribLayout.i_bindings        = CONST_bindings;
    rtRenderer::getRTFormats(&pipeDesc);
    pipeDesc.extCount                    = 0;
    pipeDesc.mainStates                  = &CONST_stateDesc;
    pipeDesc.shaderCount                 = 2;
    struct tgfx_shaderSource* shaders[2] = {firstVertShader, fragmentShader};
    pipeDesc.shaders                     = shaders;

    tgfx_bindingTableDescription tables[32] = {};
    for (uint32_t i = 0; i < props.instanceInputCount && i < 16; i++) {
      SEM_getBindingTableDesc(( rtShaderEffect* )se, props.instanceInputs[i], &tables[i]);
    }
    pipeDesc.tableCount = props.instanceInputCount;
    pipeDesc.tables     = tables;

    contentManager->createRasterPipeline(&pipeDesc, &pipe);
    SSEM_addPipeline(se, pipe, Mmt);
  }

  return 1;
}

void forwardMM_initializeManager() {
  MM_managerDesc desc;
  desc.managerName     = "Forward Mesh Manager";
  desc.managerVer      = MAKE_PLUGIN_VERSION_TAPI(0, 0, 0);
  desc.allocateMeshFnc = forwardMesh_allocateFnc;
  desc.frameFnc        = forwardMesh_frameFnc;
  desc.renderMeshFnc   = forwardMesh_renderFnc;
  desc.uploadMeshFnc   = forwardMesh_uploadFnc;
  desc.destroyMeshFnc  = forwardMesh_destroyFnc;
  desc.supportsSEFnc   = forwardMesh_supportsSEfnc;
  desc.meshStructSize  = sizeof(rtForwardMesh);
  Mmt                  = MM_registerManager(desc);

  gpuVertexBuffer = rtRenderer::allocateMemoryBlock(bufferUsageMask_tgfx_COPYTO |
                                                      bufferUsageMask_tgfx_VERTEXBUFFER |
                                                      bufferUsageMask_tgfx_STORAGEBUFFER,
                                                    16ull << 20, rtMemoryRegionType_LOCAL);

  gpuIndexBuffer =
    rtRenderer::allocateMemoryBlock(bufferUsageMask_tgfx_COPYTO | bufferUsageMask_tgfx_INDEXBUFFER |
                                      bufferUsageMask_tgfx_STORAGEBUFFER,
                                    16ull << 20, rtMemoryRegionType_LOCAL);

  gpuDrawIndirectBuffer = rtRenderer::allocateMemoryBlock(
    bufferUsageMask_tgfx_COPYTO | bufferUsageMask_tgfx_INDIRECTBUFFER |
      bufferUsageMask_tgfx_STORAGEBUFFER,
    RT_MAXDEFAULTMESH_MESHCOUNT * sizeof(tgfx_drawIndexedIndirectArgument), rtMemoryRegionType_UPLOAD);

  gpuComputeIndirectBuffer = rtRenderer::allocateMemoryBlock(
    bufferUsageMask_tgfx_COPYTO | bufferUsageMask_tgfx_INDIRECTBUFFER,
    RT_MAXDEFAULTMESH_MESHCOUNT * sizeof(tgfx_dispatchIndirectArgument), rtMemoryRegionType_UPLOAD);

  for (uint32_t i = 0; i < swapchainTextureCount; i++) {
    transformsBuffer[i] = rtRenderer::allocateMemoryBlock(
      bufferUsageMask_tgfx_COPYTO | bufferUsageMask_tgfx_STORAGEBUFFER, RT_MAXTRANSFORMBUFFER_SIZE,
      rtMemoryRegionType_UPLOAD);
  }

  // Compile vertex shader
  {
    const char* vertShaderText = ( const char* )fileSys->read_textfile(
      string_type_tapi_UTF8, SOURCE_DIR "Content/firstShader.vert", string_type_tapi_UTF8);
    contentManager->compileShaderSource(gpu, shaderlanguages_tgfx_GLSL,
                                        shaderStage_tgfx_VERTEXSHADER, vertShaderText,
                                        strlen(vertShaderText), &firstVertShader);
  }

  // Create binding types
  {
    transformBindingDesc                    = {};
    transformBindingDesc.descriptorType     = shaderdescriptortype_tgfx_BUFFER;
    transformBindingDesc.elementCount       = RT_MAXDEFAULTMESH_MESHCOUNT;
    transformBindingDesc.staticSamplerCount = 0;
    transformBindingDesc.visibleStagesMask =
      shaderStage_tgfx_VERTEXSHADER | shaderStage_tgfx_COMPUTESHADER;
    transformBindingDesc.isDynamic = true;

    indirectDrawBindingTableDesc                    = {};
    indirectDrawBindingTableDesc.descriptorType     = shaderdescriptortype_tgfx_BUFFER;
    indirectDrawBindingTableDesc.elementCount       = 1;
    indirectDrawBindingTableDesc.isDynamic          = true;
    indirectDrawBindingTableDesc.staticSamplerCount = 0;
    indirectDrawBindingTableDesc.visibleStagesMask  = shaderStage_tgfx_COMPUTESHADER;
    contentManager->createBindingTable(gpu, &indirectDrawBindingTableDesc,
                                       &indirectDrawBindingTable);
    uint32_t            bindingIndx = 0;
    struct tgfx_buffer* drawIndirectBufferTgfx =
      rtRenderer::getBufferTgfxHnd(gpuDrawIndirectBuffer);
    contentManager->setBindingTable_Buffer(indirectDrawBindingTable, 1, &bindingIndx,
                                           &drawIndirectBufferTgfx, &bindingIndx, nullptr, 0,
                                           nullptr);
  }

  // Compile compute pipeline
  {
    const char* shaderText = ( const char* )fileSys->read_textfile(
      string_type_tapi_UTF8, SOURCE_DIR "Content/firstComputeShader.comp", string_type_tapi_UTF8);
    struct tgfx_shaderSource* firstComputeShader = nullptr;
    contentManager->compileShaderSource(gpu, shaderlanguages_tgfx_GLSL,
                                        shaderStage_tgfx_COMPUTESHADER, ( void* )shaderText,
                                        strlen(shaderText), &firstComputeShader);

    tgfx_bindingTableDescription computeBTs[6] = {*rtRenderer::getSwapchainStorageBindingDesc(),
                                                  *rtRenderer::getCamBindingDesc(),
                                                  transformBindingDesc,
                                                  indirectDrawBindingTableDesc,
                                                  transformBindingDesc,
                                                  transformBindingDesc};
    contentManager->createComputePipeline(firstComputeShader,
                                          sizeof(computeBTs) / sizeof(computeBTs[0]), computeBTs,
                                          false, &meshComputePipeline);
  }

  // Prepare raster pipeline settings
  {
    {
      uint32_t elementOffset = 0;
      for (uint32_t i = 0; i < attribCount; i++) {
        auto& attrib         = CONST_attribs[i];
        attrib.attributeIndx = i;
        attrib.bindingIndx   = 0;
        attrib.dataType      = attribElementTypesTgfx[i];
        attrib.offset        = elementOffset;
        elementOffset += attribElementSize[i];
      }

      CONST_bindings[0].bindingIndx = 0;
      CONST_bindings[0].inputRate   = vertexBindingInputRate_tgfx_VERTEX;
      CONST_bindings[0].stride      = elementOffset;
    }

    CONST_stateDesc.culling                             = cullmode_tgfx_BACK;
    CONST_stateDesc.polygonmode                         = polygonmode_tgfx_FILL;
    CONST_stateDesc.topology                            = vertexlisttypes_tgfx_TRIANGLELIST;
    CONST_stateDesc.depthStencilState.depthTestEnabled  = true;
    CONST_stateDesc.depthStencilState.depthWriteEnabled = true;
    CONST_stateDesc.depthStencilState.depthCompare      = compare_tgfx_LEQUAL;
    CONST_stateDesc.blendStates[0].blendEnabled         = false;
    CONST_stateDesc.blendStates[0].blendComponents      = textureComponentMask_tgfx_ALL;
    CONST_stateDesc.blendStates[0].alphaMode            = blendmode_tgfx_MAX;
    CONST_stateDesc.blendStates[0].colorMode            = blendmode_tgfx_ADDITIVE;
    CONST_stateDesc.blendStates[0].dstAlphaFactor       = blendfactor_tgfx_DST_ALPHA;
    CONST_stateDesc.blendStates[0].srcAlphaFactor       = blendfactor_tgfx_SRC_ALPHA;
    CONST_stateDesc.blendStates[0].dstColorFactor       = blendfactor_tgfx_DST_COLOR;
    CONST_stateDesc.blendStates[0].srcColorFactor       = blendfactor_tgfx_SRC_COLOR;
  }

  // Fill indirect operation type list
  for (uint32_t i = 0; i < RT_MAXDEFAULTMESH_MESHCOUNT; i++) {
    rasterOperationList[i] = indirectOperationType_tgfx_DRAWINDEXED;
    rtOperationList[i]     = indirectOperationType_tgfx_DISPATCH;
  }

  // Create binding tables for each swapchain texture
  for (uint32_t i = 0; i < swapchainTextureCount; i++) {
    contentManager->createBindingTable(gpu, &transformBindingDesc, &transformBindingTables[i]);
    contentManager->createBindingTable(gpu, &transformBindingDesc, &vertexBufferBindingTables[i]);
    contentManager->createBindingTable(gpu, &transformBindingDesc, &indexBufferBindingTables[i]);
  }
}