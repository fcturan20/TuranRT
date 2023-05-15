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

#include "../render_context/rendercontext.h"
#include "../resourceSys/resourceManager.h"
#include "../editor_includes.h"
#include "../resourceSys/mesh.h"
#include "forwardMesh.h"

char_c                         attribNames[]            = {"POSITION", "TEXCOORD_0", "NORMAL"};
uint32_sc                       attribCount              = length_c(attribNames);
uint32_sc                       attribElementSize[]      = {3 * 4, 2 * 4, 3 * 4};
static constexpr datatype_tgfx attribElementTypesTgfx[] = {
  datatype_tgfx_VAR_VEC3, datatype_tgfx_VAR_VEC2, datatype_tgfx_VAR_VEC3};
static_assert(length_c(attribElementSize) == attribCount &&
                length_c(attribElementTypesTgfx) == attribCount,
              "Attribute lists isn't matching");

// Maximum number of seperate meshes
// It's important because each mesh will operate a instanced indirect draw call
static constexpr uint32_t RT_MAXDEFAULTMESH_MESHCOUNT = 1024, RT_MAXTRANSFORMBUFFER_SIZE = 4 << 20;

rtGpuMemBlock gpuVertexBuffer = {}, gpuIndexBuffer = {}, gpuDrawIndirectBuffer = {},
              gpuComputeIndirectBuffer = {}, transformsBuffer[swapchainTextureCount] = {};
std::vector<commandBundle_tgfxhnd> uploadBundleList   = {};
pipeline_tgfxhnd                   meshRasterPipeline = {}, meshComputePipeline = {};
bindingTableDescription_tgfx       transformBindingDesc = {}, indirectDrawBindingTableDesc;
bindingTable_tgfxhnd               transformBindingTables[swapchainTextureCount] = {},
                     indirectDrawBindingTable                                    = {},
                     vertexBufferBindingTables[swapchainTextureCount]            = {},
                     indexBufferBindingTables[swapchainTextureCount]             = {};
indirectOperationType_tgfx rasterOperationList[RT_MAXDEFAULTMESH_MESHCOUNT]      = {},
                           rtOperationList[RT_MAXDEFAULTMESH_MESHCOUNT]          = {};
std::vector<commandBundle_tgfxhnd> m_bundles[swapchainTextureCount];
rtMeshManagerType                  Mmt = {};

struct forwardMesh_rt {
  uint32_t m_gpuVertexOffset = 0, m_gpuIndexOffset = 0;
  bool     isAlive = false;
  // Optional parameters
  // Because mesh data will be in GPU-side as default
  // If user wants to access buffer, GPU->CPU copy is necessary
  void*         m_data;
  uint32_t      m_vertexCount, m_indexCount;
  rtGpuMemBlock m_stagingBlock;
};
typedef struct forwardMesh_rt* rtForwardMesh;
std::vector<rtForwardMesh>     meshes = {}, uploadQueue = {};

rtMeshManagerType forwardMeshManager_rt::managerType() { return Mmt; }
rtMesh            forwardMeshManager_rt::allocateMesh(uint32_t vertexCount, uint32_t indexCount,
                                                      void** meshData) {
  uint32_t      maxVertexOffset = 0, maxIndexOffset = 0;
  rtForwardMesh mesh = {};
  for (rtForwardMesh searchMesh : meshes) {
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
    mesh                    = ( rtForwardMesh )rtMeshManager::createMeshHandle(Mmt);
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
                                                                    stagingBufferSize, rtRenderer::UPLOAD);

  // Can't use staging memory, keep it in RAM to upload later
  if (mesh->m_stagingBlock) {
    mesh->m_data = rtRenderer::getBufferMappedMemPtr(mesh->m_stagingBlock);
  } else {
    mesh->m_data = malloc(stagingBufferSize);
  }
  *meshData = mesh->m_data;

  return ( rtMesh )mesh;
}

unsigned char forwardMeshManager_rt::forwardMesh_uploadFnc(rtMesh i_mesh) {
  rtForwardMesh mesh = ( rtForwardMesh )i_mesh;
  // If there is no space in upload memory, push to queue to upload later
  if (!mesh->m_stagingBlock) {
    uploadQueue.push_back(mesh);
    return false;
  }

  commandBundle_tgfxhnd copyBundle = renderer->beginCommandBundle(gpu, 2, nullptr, 0, nullptr);
  renderer->cmdCopyBufferToBuffer(copyBundle, 0, mesh->m_vertexCount * sizeof(rtForwardVertex),
                                  rtRenderer::getBufferTgfxHnd(mesh->m_stagingBlock), 0,
                                  rtRenderer::getBufferTgfxHnd(gpuVertexBuffer),
                                  mesh->m_gpuVertexOffset * sizeof(rtForwardVertex));
  renderer->cmdCopyBufferToBuffer(copyBundle, 1, mesh->m_indexCount * sizeof(uint32_t),
                                  rtRenderer::getBufferTgfxHnd(mesh->m_stagingBlock),
                                  mesh->m_vertexCount * sizeof(rtForwardVertex),
                                  rtRenderer::getBufferTgfxHnd(gpuIndexBuffer),
                                  mesh->m_gpuIndexOffset * sizeof(uint32_t));
  renderer->finishCommandBundle(copyBundle, 0, nullptr);
  uploadBundleList.push_back(copyBundle);
  return true;
}
unsigned char forwardMeshManager_rt::forwardMesh_destroyFnc(rtMesh) { return true; }

struct renderList {
  rtForwardMesh        m_mesh;
  std::vector<mat4_rt> m_transforms;
};
commandBundle_tgfxhnd forwardMeshManager_rt::forwardMesh_renderFnc(
  unsigned int count, const rtMeshManager::renderInfo* const infos) {
  uint32_t frameIndx = rtRenderer::getFrameIndx();

  if (m_bundles[frameIndx].size()) {
    for (commandBundle_tgfxhnd bndle : m_bundles[frameIndx]) {
      renderer->destroyCommandBundle(bndle);
    }
  }

  m_bundles[frameIndx] = uploadBundleList;
  uploadBundleList.clear();

  // Create render list to fill binding tables
  std::vector<renderList> renders;
  for (uint32_t infoIndx = 0; infoIndx < count; infoIndx++) {
    bool isFound = false;
    for (renderList& r : renders) {
      if (r.m_mesh == ( rtForwardMesh )infos[infoIndx].mesh) {
        r.m_transforms.push_back(*infos[infoIndx].transform);
        isFound = true;
        break;
      }
    }
    if (!isFound) {
      renderList r = {};
      r.m_mesh     = ( rtForwardMesh )infos[infoIndx].mesh;
      r.m_transforms.push_back(*infos[infoIndx].transform);
      renders.push_back(r);
    }
  }

  struct bindingTableUpdate {
    // Batched set infos
    buffer_tgfxhnd batchBuffers[RT_MAXDEFAULTMESH_MESHCOUNT] = {};
    uint32_t       batchIndices[RT_MAXDEFAULTMESH_MESHCOUNT] = {},
             batchOffsets[RT_MAXDEFAULTMESH_MESHCOUNT]       = {},
             batchSizes[RT_MAXDEFAULTMESH_MESHCOUNT]         = {};
  };
  // Fill transform & draw indirect buffer & binding table
  {
    uint32_t       transformIndx        = 0;
    buffer_tgfxhnd transformsBufferTgfx = rtRenderer::getBufferTgfxHnd(transformsBuffer[frameIndx]);
    glm::mat4*     transformsBufferMapped =
      ( glm::mat4* )rtRenderer::getBufferMappedMemPtr(transformsBuffer[frameIndx]);
    tgfx_indirect_argument_draw_indexed* drawIndirectBufferMapped =
      ( tgfx_indirect_argument_draw_indexed* )rtRenderer::getBufferMappedMemPtr(
        gpuDrawIndirectBuffer);

    bindingTableUpdate btUpdate;

    for (uint32_t rIndx = 0; rIndx < renders.size(); rIndx++) {
      tgfx_indirect_argument_draw_indexed lastIndirectBuffer = {};
      const renderList&                   r                  = renders[rIndx];

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
    bindingTableUpdate vbUpdate, ibUpdate;
    buffer_tgfxhnd     vbTgfx = rtRenderer::getBufferTgfxHnd(gpuVertexBuffer),
                   ibTgfx     = rtRenderer::getBufferTgfxHnd(gpuIndexBuffer);

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

  commandBundle_tgfxhnd bundle =
    renderer->beginCommandBundle(gpu, cmdCount, meshRasterPipeline, 0, nullptr);
  uint32_t             cmdIndx          = 0;
  bindingTable_tgfxhnd bindingTables[2] = {rtRenderer::getActiveCamBindingTable(),
                                           transformBindingTables[frameIndx]};
  renderer->cmdSetDepthBounds(bundle, cmdIndx++, 0.0f, 1.0f);
  renderer->cmdSetScissor(bundle, cmdIndx++, {0, 0}, {1280, 720});
  viewportInfo_tgfx viewport;
  viewport.depthMinMax   = {0.0f, 1.0f};
  viewport.size          = {1280, 720};
  viewport.topLeftCorner = {0, 0};
  renderer->cmdSetViewport(bundle, cmdIndx++, viewport);
  renderer->cmdBindBindingTables(bundle, cmdIndx++, 0, 2, bindingTables, pipelineType_tgfx_RASTER);
  uint64_t       offset         = 0;
  buffer_tgfxhnd vertBuffer     = rtRenderer::getBufferTgfxHnd(gpuVertexBuffer),
                 indxBuffer     = rtRenderer::getBufferTgfxHnd(gpuIndexBuffer),
                 indirectBuffer = rtRenderer::getBufferTgfxHnd(gpuDrawIndirectBuffer);
  renderer->cmdBindVertexBuffers(bundle, cmdIndx++, 0, 1, &vertBuffer, &offset);
  renderer->cmdBindIndexBuffer(bundle, cmdIndx++, indxBuffer, 0, 4);
  renderer->cmdExecuteIndirect(bundle, cmdIndx++, renders.size(), rasterOperationList,
                               indirectBuffer, 0, 0, nullptr);
  assert(cmdIndx <= cmdCount && "Cmd count is exceeded!");
  renderer->finishCommandBundle(bundle, 0, nullptr);
  m_bundles[frameIndx].push_back(bundle);
  return bundle;
}
void forwardMeshManager_rt::forwardMesh_frameFnc() {
  for (commandBundle_tgfxhnd bndl : uploadBundleList) {
    rtRenderer::upload(bndl);
  }

  // Clear staging memory uploads, then copy queued meshes to staging memory
}

rtMesh forwardMesh_allocateFnc(uint32_t vertexCount, uint32_t indexCount, void* extraInfo,
                               void** meshData) {
  return rtForwardMeshManager::allocateMesh(vertexCount, indexCount, meshData);
}
void forwardMeshManager_rt::initializeManager() {
  rtMeshManager::managerDesc desc;
  desc.managerName     = "Forward Mesh Manager";
  desc.managerVer      = MAKE_PLUGIN_VERSION_TAPI(0, 0, 0);
  desc.allocateMeshFnc = forwardMesh_allocateFnc;
  desc.frameFnc        = forwardMesh_frameFnc;
  desc.renderMeshFnc   = forwardMesh_renderFnc;
  desc.uploadMeshFnc   = forwardMesh_uploadFnc;
  desc.destroyMeshFnc  = forwardMesh_destroyFnc;
  desc.meshStructSize  = sizeof(forwardMesh_rt);
  Mmt                  = rtMeshManager::registerManager(desc);

  gpuVertexBuffer = rtRenderer::allocateMemoryBlock(bufferUsageMask_tgfx_COPYTO |
                                                      bufferUsageMask_tgfx_VERTEXBUFFER |
                                                      bufferUsageMask_tgfx_STORAGEBUFFER,
                                                    16ull << 20);

  gpuIndexBuffer =
    rtRenderer::allocateMemoryBlock(bufferUsageMask_tgfx_COPYTO | bufferUsageMask_tgfx_INDEXBUFFER |
                                      bufferUsageMask_tgfx_STORAGEBUFFER,
                                    16ull << 20);

  gpuDrawIndirectBuffer = rtRenderer::allocateMemoryBlock(
    bufferUsageMask_tgfx_COPYTO | bufferUsageMask_tgfx_INDIRECTBUFFER |
      bufferUsageMask_tgfx_STORAGEBUFFER,
    RT_MAXDEFAULTMESH_MESHCOUNT * sizeof(tgfx_indirect_argument_draw_indexed), rtRenderer::UPLOAD);

  gpuComputeIndirectBuffer = rtRenderer::allocateMemoryBlock(
    bufferUsageMask_tgfx_COPYTO | bufferUsageMask_tgfx_INDIRECTBUFFER,
    RT_MAXDEFAULTMESH_MESHCOUNT * sizeof(tgfx_indirect_argument_dispatch), rtRenderer::UPLOAD);

  for (uint32_t i = 0; i < swapchainTextureCount; i++) {
    transformsBuffer[i] = rtRenderer::allocateMemoryBlock(
      bufferUsageMask_tgfx_COPYTO | bufferUsageMask_tgfx_STORAGEBUFFER, RT_MAXTRANSFORMBUFFER_SIZE,
      rtRenderer::UPLOAD);
  }

  // Compile vertex shader
  shaderSource_tgfxhnd firstVertShader;
  {
    const char* vertShaderText = ( const char* )fileSys->read_textfile(
      string_type_tapi_UTF8, SOURCE_DIR "Content/firstShader.vert", string_type_tapi_UTF8);
    contentManager->compileShaderSource(gpu, shaderlanguages_tgfx_GLSL,
                                        shaderStage_tgfx_VERTEXSHADER, ( void* )vertShaderText,
                                        strlen(vertShaderText), &firstVertShader);
  }

  // Create binding types
  {
    transformBindingDesc                    = {};
    transformBindingDesc.DescriptorType     = shaderdescriptortype_tgfx_BUFFER;
    transformBindingDesc.ElementCount       = RT_MAXDEFAULTMESH_MESHCOUNT;
    transformBindingDesc.staticSamplerCount = 0;
    transformBindingDesc.visibleStagesMask =
      shaderStage_tgfx_VERTEXSHADER | shaderStage_tgfx_COMPUTESHADER;
    transformBindingDesc.isDynamic = true;

    indirectDrawBindingTableDesc                    = {};
    indirectDrawBindingTableDesc.DescriptorType     = shaderdescriptortype_tgfx_BUFFER;
    indirectDrawBindingTableDesc.ElementCount       = 1;
    indirectDrawBindingTableDesc.isDynamic          = true;
    indirectDrawBindingTableDesc.staticSamplerCount = 0;
    indirectDrawBindingTableDesc.visibleStagesMask  = shaderStage_tgfx_COMPUTESHADER;
    contentManager->createBindingTable(gpu, &indirectDrawBindingTableDesc,
                                       &indirectDrawBindingTable);
    uint32_t       bindingIndx            = 0;
    buffer_tgfxhnd drawIndirectBufferTgfx = rtRenderer::getBufferTgfxHnd(gpuDrawIndirectBuffer);
    contentManager->setBindingTable_Buffer(indirectDrawBindingTable, 1, &bindingIndx,
                                           &drawIndirectBufferTgfx, &bindingIndx, nullptr, 0,
                                           nullptr);
  }

  // Compile compute pipeline
  {
    const char* shaderText = ( const char* )fileSys->read_textfile(
      string_type_tapi_UTF8, SOURCE_DIR "Content/firstComputeShader.comp", string_type_tapi_UTF8);
    shaderSource_tgfxhnd firstComputeShader = nullptr;
    contentManager->compileShaderSource(gpu, shaderlanguages_tgfx_GLSL,
                                        shaderStage_tgfx_COMPUTESHADER, ( void* )shaderText,
                                        strlen(shaderText), &firstComputeShader);

    bindingTableDescription_tgfx computeBTs[6] = {*rtRenderer::getSwapchainStorageBindingDesc(),
                                                  *rtRenderer::getCamBindingDesc(),
                                                  transformBindingDesc,
                                                  indirectDrawBindingTableDesc,
                                                  transformBindingDesc,
                                                  transformBindingDesc};
    contentManager->createComputePipeline(firstComputeShader,
                                          sizeof(computeBTs) / sizeof(computeBTs[0]), computeBTs,
                                          false, &meshComputePipeline);
  }

  // Compile raster pipeline
  {
    vertexAttributeDescription_tgfx attribs[attribCount];
    vertexBindingDescription_tgfx   bindings[1];
    {
      uint32_t elementOffset = 0;
      for (uint32_t i = 0; i < attribCount; i++) {
        auto& attrib         = attribs[i];
        attrib.attributeIndx = i;
        attrib.bindingIndx   = 0;
        attrib.dataType      = attribElementTypesTgfx[i];
        attrib.offset        = elementOffset;
        elementOffset += attribElementSize[i];
      }

      bindings[0].bindingIndx = 0;
      bindings[0].inputRate   = vertexBindingInputRate_tgfx_VERTEX;
      bindings[0].stride      = elementOffset;
    }

    rasterStateDescription_tgfx stateDesc         = {};
    stateDesc.culling                             = cullmode_tgfx_BACK;
    stateDesc.polygonmode                         = polygonmode_tgfx_FILL;
    stateDesc.topology                            = vertexlisttypes_tgfx_TRIANGLELIST;
    stateDesc.depthStencilState.depthTestEnabled  = true;
    stateDesc.depthStencilState.depthWriteEnabled = true;
    stateDesc.depthStencilState.depthCompare      = compare_tgfx_LEQUAL;
    stateDesc.blendStates[0].blendEnabled         = false;
    stateDesc.blendStates[0].blendComponents      = textureComponentMask_tgfx_ALL;
    stateDesc.blendStates[0].alphaMode            = blendmode_tgfx_MAX;
    stateDesc.blendStates[0].colorMode            = blendmode_tgfx_ADDITIVE;
    stateDesc.blendStates[0].dstAlphaFactor       = blendfactor_tgfx_DST_ALPHA;
    stateDesc.blendStates[0].srcAlphaFactor       = blendfactor_tgfx_SRC_ALPHA;
    stateDesc.blendStates[0].dstColorFactor       = blendfactor_tgfx_DST_COLOR;
    stateDesc.blendStates[0].srcColorFactor       = blendfactor_tgfx_SRC_COLOR;
    rasterPipelineDescription_tgfx pipelineDesc   = {};
    rtRenderer::getRTFormats(&pipelineDesc);
    pipelineDesc.mainStates                = &stateDesc;
    shaderSource_tgfxhnd shaderSources[2]  = {firstVertShader, rtRenderer::getDefaultFragShader()};
    pipelineDesc.shaderCount               = 2;
    pipelineDesc.shaders                   = shaderSources;
    pipelineDesc.attribLayout.attribCount  = 3;
    pipelineDesc.attribLayout.bindingCount = 1;
    pipelineDesc.attribLayout.i_attributes = attribs;
    pipelineDesc.attribLayout.i_bindings   = bindings;
    bindingTableDescription_tgfx bindingTypes[2] = {*rtRenderer::getCamBindingDesc(),
                                                    transformBindingDesc};
    pipelineDesc.tables                          = bindingTypes;
    pipelineDesc.tableCount                      = 2;

    contentManager->createRasterPipeline(&pipelineDesc, &meshRasterPipeline);
    contentManager->destroyShaderSource(firstVertShader);
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