#include <assimp/postprocess.h>
#include <assimp/scene.h>
#include <assimp/Importer.hpp>
#include <glm/glm.hpp>
#include <vector>
#include <tgfx_forwarddeclarations.h>
#include <tgfx_renderer.h>
#include <tgfx_structs.h>

#include <tgfx_structs.h>
#include "../render_context/rendercontext.h"
#include "resourceManager.h"
#include "../editor_includes.h"
#include "mesh.h"

// Maximum number of seperate meshes
// It's important because each mesh will operate a instanced indirect draw call
static constexpr uint32_t RT_MAXDEFAULTMESH_MESHCOUNT = 1024;
struct rtMeshManager_private {
  static std::vector<rtMesh>                meshes, uploadQueue;
  static rtResourceManagerType              managerType;
  static rtGpuMemBlock                      gpuVertexBuffer, gpuIndexBuffer, gpuIndirectBuffer;
  static std::vector<commandBundle_tgfxhnd> uploadBundleList;

  static bool deserializeMesh(rtResourceDesc* desc) { return false; }
  static bool isMeshValid(void* dataHnd) { return false; }
};
rtResourceManagerType rtMeshManager_private::managerType     = {};
rtGpuMemBlock rtMeshManager_private::gpuVertexBuffer   = {},
              rtMeshManager_private::gpuIndexBuffer    = {},
              rtMeshManager_private::gpuIndirectBuffer       = {};
std::vector<rtMesh> rtMeshManager_private::meshes = {}, rtMeshManager_private::uploadQueue = {};
std::vector<commandBundle_tgfxhnd> rtMeshManager_private::uploadBundleList = {};

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
  mesh->m_stagingBlock       = rtRenderer::allocateMemoryBlock(
          bufferUsageMask_tgfx_COPYFROM, stagingBufferSize, rtRenderer::UPLOAD);

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
    RT_MAXDEFAULTMESH_MESHCOUNT * sizeof(tgfx_indirect_argument_draw_indexed));
}