#include <assimp/postprocess.h>
#include <assimp/scene.h>
#include <assimp/Importer.hpp>
#include <glm/glm.hpp>
#include <vector>
#include <filesys_tapi.h>
#include <tgfx_core.h>
#include <tgfx_forwarddeclarations.h>
#include <tgfx_renderer.h>
#include <tgfx_gpucontentmanager.h>
#include <tgfx_structs.h>

#include <tgfx_structs.h>
#include "../render_context/rendercontext.h"
#include "resourceManager.h"
#include "../editor_includes.h"
#include "mesh.h"

// Maximum number of seperate meshes
// It's important because each mesh will operate a instanced indirect draw call
static constexpr uint32_t RT_MAXDEFAULTMESH_MESHCOUNT = 1024, RT_MAXTRANSFORMBUFFER_SIZE = 4 << 20;
struct rtMeshManager_private {
  static std::vector<rtMesh>   meshes, uploadQueue;
  static rtResourceManagerType managerType;
  static rtGpuMemBlock         gpuVertexBuffer, gpuIndexBuffer, gpuIndirectBuffer,
    gpuShaderReadBuffer[swapchainTextureCount];
  static std::vector<commandBundle_tgfxhnd> uploadBundleList;
  static pipeline_tgfxhnd                   defaultPipeline;
  static bindingTableDescription_tgfx       transformBindingDesc;
  static bindingTable_tgfxhnd               transformBindingTables[swapchainTextureCount];
  static indirectOperationType_tgfx         operationList[RT_MAXDEFAULTMESH_MESHCOUNT];
  static commandBundle_tgfxhnd              m_bundles[swapchainTextureCount];

  static bool deserializeMesh(rtResourceDesc* desc) { return false; }
  static bool isMeshValid(void* dataHnd) { return false; }
};
rtResourceManagerType rtMeshManager_private::managerType                        = {};
rtGpuMemBlock         rtMeshManager_private::gpuVertexBuffer                    = {},
              rtMeshManager_private::gpuIndexBuffer                             = {},
              rtMeshManager_private::gpuIndirectBuffer                          = {},
              rtMeshManager_private::gpuShaderReadBuffer[swapchainTextureCount] = {};
std::vector<rtMesh> rtMeshManager_private::meshes = {}, rtMeshManager_private::uploadQueue = {};
std::vector<commandBundle_tgfxhnd> rtMeshManager_private::uploadBundleList                   = {};
pipeline_tgfxhnd                   rtMeshManager_private::defaultPipeline                    = {};
bindingTableDescription_tgfx       rtMeshManager_private::transformBindingDesc               = {};
bindingTable_tgfxhnd rtMeshManager_private::transformBindingTables[swapchainTextureCount]    = {};
indirectOperationType_tgfx rtMeshManager_private::operationList[RT_MAXDEFAULTMESH_MESHCOUNT] = {};
commandBundle_tgfxhnd      rtMeshManager_private::m_bundles[swapchainTextureCount];

struct mesh_rt {
  uint32_t m_gpuVertexOffset = 0, m_gpuIndexOffset = 0;
  bool     isAlive = false;
  // Optional parameters
  // Because mesh data will be in GPU-side as default
  // If user wants to access buffer, GPU->CPU copy is necessary
  void*         m_data;
  uint32_t      m_vertexCount, m_indexCount;
  rtGpuMemBlock m_stagingBlock;
};

rtMesh meshManager_rt::allocateMesh(uint32_t vertexCount, uint32_t indexCount, void** meshData) {
  uint32_t maxVertexOffset = 0, maxIndexOffset = 0;
  rtMesh   mesh = {};
  for (rtMesh searchMesh : rtMeshManager_private::meshes) {
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
    mesh                    = new mesh_rt;
    mesh->m_gpuVertexOffset = maxVertexOffset;
    mesh->m_gpuIndexOffset  = maxIndexOffset;
    rtMeshManager_private::meshes.push_back(mesh);
  }
  mesh->isAlive       = true;
  mesh->m_vertexCount = vertexCount;
  mesh->m_indexCount  = indexCount;

  uint32_t stagingBufferSize = (vertexCount * sizeof(rtVertex)) + (indexCount * sizeof(uint32_t));
  mesh->m_stagingBlock       = rtRenderer::allocateMemoryBlock(bufferUsageMask_tgfx_COPYFROM,
                                                               stagingBufferSize, rtRenderer::UPLOAD);

  // Can't use staging memory, keep it in RAM to upload later
  if (mesh->m_stagingBlock) {
    mesh->m_data = rtRenderer::getBufferMappedMemPtr(mesh->m_stagingBlock);
  } else {
    mesh->m_data = malloc(stagingBufferSize);
  }
  *meshData = mesh->m_data;

  return mesh;
}

void meshManager_rt::uploadMesh(rtMesh mesh) {
  // If there is no space in upload memory, push to queue to upload later
  if (!mesh->m_stagingBlock) {
    rtMeshManager_private::uploadQueue.push_back(mesh);
    return;
  }

  commandBundle_tgfxhnd copyBundle = renderer->beginCommandBundle(gpu, 2, {}, {});
  renderer->cmdCopyBufferToBuffer(
    copyBundle, 0, mesh->m_vertexCount * sizeof(rtVertex),
    rtRenderer::getBufferTgfxHnd(mesh->m_stagingBlock), 0,
    rtRenderer::getBufferTgfxHnd(rtMeshManager_private::gpuVertexBuffer),
    mesh->m_gpuVertexOffset * sizeof(rtVertex));
  renderer->cmdCopyBufferToBuffer(
    copyBundle, 1, mesh->m_indexCount * sizeof(uint32_t),
    rtRenderer::getBufferTgfxHnd(mesh->m_stagingBlock), mesh->m_vertexCount * sizeof(rtVertex),
    rtRenderer::getBufferTgfxHnd(rtMeshManager_private::gpuIndexBuffer),
    mesh->m_gpuIndexOffset * sizeof(uint32_t));
  renderer->finishCommandBundle(copyBundle, {});
  rtMeshManager_private::uploadBundleList.push_back(copyBundle);
}

rtMesh meshManager_rt::createDefaultMesh(aiMesh* aMesh) {
  void*  meshData = nullptr;
  rtMesh mesh = rtMeshManager::allocateMesh(aMesh->mNumVertices, aMesh->mNumFaces * 3, &meshData);
  rtMeshManager_private::meshes.push_back(mesh);
  rtVertex* vertexBuffer = ( rtVertex* )meshData;
  uint32_t* indexBuffer =
    ( uint32_t* )((( uintptr_t )vertexBuffer) + (aMesh->mNumVertices * sizeof(rtVertex)));

  for (uint32_t v = 0; v < aMesh->mNumVertices; v++) {
    vertexBuffer[v].pos =
      glm::vec3(aMesh->mVertices[v].x, aMesh->mVertices[v].y, aMesh->mVertices[v].z);
  }
  for (uint32_t v = 0; v < aMesh->mNumVertices; v++) {
    vertexBuffer[v].normal =
      glm::vec3(aMesh->mNormals[v].x, aMesh->mNormals[v].y, aMesh->mNormals[v].z);
  }
  for (uint32_t v = 0; v < aMesh->mNumVertices; v++) {
    if (aMesh->mTextureCoords[0]) {
      vertexBuffer[v].textCoord =
        glm::vec3(aMesh->mTextureCoords[0][v].x, aMesh->mTextureCoords[0][v].y,
                  aMesh->mTextureCoords[0][v].z);
    }
  }

  for (uint32_t faceIndx = 0; faceIndx < aMesh->mNumFaces; faceIndx++) {
    if (!aMesh->mFaces[faceIndx].mIndices) {
      continue;
    }
    if (aMesh->mFaces[faceIndx].mNumIndices != 3) {
      assert(0 && "One of the faces isn't triangulated!");
    }
    for (uint32_t v = 0; v < 3; v++) {
      indexBuffer[(faceIndx * 3) + v] = aMesh->mFaces[faceIndx].mIndices[v];
    }
  }
  uploadMesh(mesh);
  return mesh;
}

struct renderList {
  rtMesh                 m_mesh;
  std::vector<glm::mat4> m_transforms;
};
commandBundle_tgfxhnd meshManager_rt::render(unsigned int count, renderInfo* const infos) {
  uint32_t frameIndx = rtRenderer::getFrameIndx();

  if (rtMeshManager_private::m_bundles[frameIndx]) {
    renderer->destroyCommandBundle(rtMeshManager_private::m_bundles[frameIndx]);
  }

  // Create render list to fill binding tables
  std::vector<renderList> renders;
  for (uint32_t infoIndx = 0; infoIndx < count; infoIndx++) {
    bool isFound = false;
    for (renderList& r : renders) {
      if (r.m_mesh == infos[infoIndx].mesh) {
        r.m_transforms.push_back(*infos[infoIndx].transform);
        isFound = true;
        break;
      }
    }
    if (!isFound) {
      renderList r = {};
      r.m_mesh     = infos[infoIndx].mesh;
      r.m_transforms.push_back(*infos[infoIndx].transform);
      renders.push_back(r);
    }
  }

  // Fill transform buffer & binding tables
  {
    uint32_t       transformIndx = 0;
    buffer_tgfxhnd shaderReadBufferTgfx =
      rtRenderer::getBufferTgfxHnd(rtMeshManager_private::gpuShaderReadBuffer[frameIndx]);
    glm::mat4* shaderReadBufferMapped = ( glm::mat4* )rtRenderer::getBufferMappedMemPtr(
      rtMeshManager_private::gpuShaderReadBuffer[frameIndx]);
    tgfx_indirect_argument_draw_indexed* indirectBufferMapped =
      ( tgfx_indirect_argument_draw_indexed* )rtRenderer::getBufferMappedMemPtr(
        rtMeshManager_private::gpuIndirectBuffer);

    // Batched set infos
    buffer_tgfxhnd batchBuffers[RT_MAXDEFAULTMESH_MESHCOUNT] = {shaderReadBufferTgfx};
    uint32_t       batchIndices[RT_MAXDEFAULTMESH_MESHCOUNT] = {},
             batchOffsets[RT_MAXDEFAULTMESH_MESHCOUNT]       = {},
             batchSizes[RT_MAXDEFAULTMESH_MESHCOUNT]         = {};
    tgfx_indirect_argument_draw_indexed lastIndirectBuffer   = {};
    for (uint32_t rIndx = 0; rIndx < renders.size(); rIndx++) {
      const renderList& r = renders[rIndx];

      batchIndices[rIndx] = rIndx;
      batchBuffers[rIndx] = shaderReadBufferTgfx;
      batchOffsets[rIndx] = transformIndx * sizeof(glm::mat4);
      batchSizes[rIndx]   = r.m_transforms.size() * sizeof(glm::mat4);

      for (glm::mat4 m : r.m_transforms) {
        shaderReadBufferMapped[transformIndx++] = m;
      }
      lastIndirectBuffer.firstIndex            = r.m_mesh->m_gpuIndexOffset;
      lastIndirectBuffer.indexCountPerInstance = r.m_mesh->m_indexCount;
      lastIndirectBuffer.instanceCount         = r.m_transforms.size();
      lastIndirectBuffer.firstInstance         = 0;
      lastIndirectBuffer.vertexOffset          = r.m_mesh->m_gpuVertexOffset;
      indirectBufferMapped[rIndx]              = lastIndirectBuffer;
    }
    for (uint32_t i = renders.size(); i < RT_MAXDEFAULTMESH_MESHCOUNT; i++) {
      batchIndices[i] = i;
      batchBuffers[i] = shaderReadBufferTgfx;
      batchOffsets[i] = 0;
      batchSizes[i]   = 1;
    }
    contentManager->setBindingTable_Buffer(rtMeshManager_private::transformBindingTables[frameIndx],
                                           RT_MAXDEFAULTMESH_MESHCOUNT, batchIndices, batchBuffers,
                                           batchOffsets, batchSizes, {});
  }

  // 1 pipeline, 2 binding tables (same call), 1 vertex buffer, 1 index buffer & 1 indirect indexed
  // draw call
  static constexpr uint32_t cmdCount = 8;

  commandBundle_tgfxhnd bundle =
    renderer->beginCommandBundle(gpu, cmdCount, rtMeshManager_private::defaultPipeline, {});
  uint32_t             cmdIndx          = 0;
  bindingTable_tgfxhnd bindingTables[3] = {rtRenderer::getActiveCamBindingTable(),
                                           rtMeshManager_private::transformBindingTables[frameIndx],
                                           ( bindingTable_tgfxhnd )tgfx->INVALIDHANDLE};
  renderer->cmdSetDepthBounds(bundle, cmdIndx++, 0.0f, 1.0f);
  renderer->cmdSetScissor(bundle, cmdIndx++, {0, 0}, {1280, 720});
  viewportInfo_tgfx viewport;
  viewport.depthMinMax   = {0.0f, 1.0f};
  viewport.size          = {1280, 720};
  viewport.topLeftCorner = {0, 0};
  renderer->cmdSetViewport(bundle, cmdIndx++, viewport);
  renderer->cmdBindPipeline(bundle, cmdIndx++, rtMeshManager_private::defaultPipeline);
  renderer->cmdBindBindingTables(bundle, cmdIndx++, bindingTables, 0, pipelineType_tgfx_RASTER);
  uint64_t       offset     = 0;
  buffer_tgfxhnd vertBuffer = rtRenderer::getBufferTgfxHnd(rtMeshManager_private::gpuVertexBuffer),
                 indxBuffer = rtRenderer::getBufferTgfxHnd(rtMeshManager_private::gpuIndexBuffer),
                 indirectBuffer =
                   rtRenderer::getBufferTgfxHnd(rtMeshManager_private::gpuIndirectBuffer);
  renderer->cmdBindVertexBuffers(bundle, cmdIndx++, 0, 1, &vertBuffer, &offset);
  renderer->cmdBindIndexBuffer(bundle, cmdIndx++, indxBuffer, 0, 4);
  renderer->cmdExecuteIndirect(bundle, cmdIndx++, renders.size(),
                               rtMeshManager_private::operationList, indirectBuffer, 0, {});
  assert(cmdIndx <= cmdCount && "Cmd count is exceeded!");
  renderer->finishCommandBundle(bundle, {});

  rtMeshManager_private::m_bundles[frameIndx] = bundle;
  return bundle;
}
void meshManager_rt::frame() {
  for (commandBundle_tgfxhnd bndl : rtMeshManager_private::uploadBundleList) {
    rtRenderer::upload(bndl);
  }

  // Clear staging memory uploads, then copy queued meshes to staging memory
}

rtResourceManagerType meshManager_rt::managerType() { return rtMeshManager_private::managerType; }

void rtMeshManager::initializeManager() {
  rtResourceManager::managerDesc desc;
  desc.managerName                   = "Default Scene Resource Manager";
  desc.managerVer                    = MAKE_PLUGIN_VERSION_TAPI(0, 0, 0);
  desc.deserialize                   = rtMeshManager_private::deserializeMesh;
  desc.validate                      = rtMeshManager_private::isMeshValid;
  rtMeshManager_private::managerType = rtResourceManager::registerManager(desc);

  rtMeshManager_private::gpuVertexBuffer = rtRenderer::allocateMemoryBlock(
    bufferUsageMask_tgfx_COPYTO | bufferUsageMask_tgfx_VERTEXBUFFER, 32ull << 20);

  rtMeshManager_private::gpuIndexBuffer = rtRenderer::allocateMemoryBlock(
    bufferUsageMask_tgfx_COPYTO | bufferUsageMask_tgfx_INDEXBUFFER, 12ull << 20);

  rtMeshManager_private::gpuIndirectBuffer = rtRenderer::allocateMemoryBlock(
    bufferUsageMask_tgfx_COPYTO | bufferUsageMask_tgfx_INDIRECTBUFFER,
    RT_MAXDEFAULTMESH_MESHCOUNT * sizeof(tgfx_indirect_argument_draw_indexed), rtRenderer::UPLOAD);

  for (uint32_t i = 0; i < swapchainTextureCount; i++) {
    rtMeshManager_private::gpuShaderReadBuffer[i] = rtRenderer::allocateMemoryBlock(
      bufferUsageMask_tgfx_COPYTO | bufferUsageMask_tgfx_STORAGEBUFFER, RT_MAXTRANSFORMBUFFER_SIZE,
      rtRenderer::UPLOAD);
  }

  // Compile vertex shader
  shaderSource_tgfxhnd firstVertShader;
  {
    const char* vertShaderText =
      filesys->funcs->read_textfile(SOURCE_DIR "Content/firstShader.vert");
    contentManager->compileShaderSource(gpu, shaderlanguages_tgfx_GLSL,
                                        shaderStage_tgfx_VERTEXSHADER, ( void* )vertShaderText,
                                        strlen(vertShaderText), &firstVertShader);
  }

  // Create binding types
  {
    rtMeshManager_private::transformBindingDesc = {};
    rtMeshManager_private::transformBindingDesc.DescriptorType = shaderdescriptortype_tgfx_BUFFER;
    rtMeshManager_private::transformBindingDesc.ElementCount   = RT_MAXDEFAULTMESH_MESHCOUNT;
    rtMeshManager_private::transformBindingDesc.SttcSmplrs     = {};
    rtMeshManager_private::transformBindingDesc.visibleStagesMask = shaderStage_tgfx_VERTEXSHADER;
    rtMeshManager_private::transformBindingDesc.isDynamic         = true;
  }

  // Compile pipeline
  {
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
    stateDesc.culling                             = cullmode_tgfx_BACK;
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
    rtRenderer::getRTFormats(&pipelineDesc);
    pipelineDesc.mainStates                = &stateDesc;
    shaderSource_tgfxhnd shaderSources[2]  = {firstVertShader, rtRenderer::getDefaultFragShader()};
    pipelineDesc.shaderSourceList          = shaderSources;
    pipelineDesc.attribLayout.attribCount  = 3;
    pipelineDesc.attribLayout.bindingCount = 1;
    pipelineDesc.attribLayout.i_attributes = attribs;
    pipelineDesc.attribLayout.i_bindings   = bindings;
    bindingTableDescription_tgfx bindingTypes[2] = {*rtRenderer::getCamBindingDesc(),
                                                    rtMeshManager_private::transformBindingDesc};
    pipelineDesc.tables                          = bindingTypes;
    pipelineDesc.tableCount                      = 2;

    contentManager->createRasterPipeline(&pipelineDesc, nullptr,
                                         &rtMeshManager_private::defaultPipeline);
    contentManager->destroyShaderSource(firstVertShader);
  }

  // Fill indirect operation type list
  for (uint32_t i = 0; i < RT_MAXDEFAULTMESH_MESHCOUNT; i++) {
    rtMeshManager_private::operationList[i] = indirectOperationType_tgfx_DRAWINDEXED;
  }

  // Create binding tables for each swapchain texture
  for (uint32_t i = 0; i < swapchainTextureCount; i++) {
    contentManager->createBindingTable(gpu, &rtMeshManager_private::transformBindingDesc,
                                       &rtMeshManager_private::transformBindingTables[i]);
  }
}