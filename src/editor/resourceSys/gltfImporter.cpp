#include <assert.h>
#include <vector>
#include <glm/glm.hpp>

#include <logger_tapi.h>
#include <string_tapi.h>
#include <ecs_tapi.h>
#include <tgfx_forwarddeclarations.h>

#include "../editor_includes.h"
#include "resourceManager.h"
#include "mesh.h"
#include "scene.h"
// #include <fastgltf_parser.hpp>
// #include <fastgltf_types.hpp>

#ifdef FASTGLTF_VERSION
static constexpr fastgltf::AccessorType attribTypes[] = {
  fastgltf::AccessorType::Vec3, fastgltf::AccessorType::Vec2, fastgltf::AccessorType::Vec3};
static_assert(length_c(attribTypes) == attribCount, "Attribute lists isn't matching");
rtMesh meshManager_rt::createDefaultMesh(const fastgltf::Asset& a, const fastgltf::Primitive& p) {
  // Index buffer check
  uint32_t iCount = UINT32_MAX;
  {
    if (!p.indicesAccessor.has_value()) {
      logSys->log(log_type_tapi_WARNING, false, L"Submesh has no index buffer, skipped it");
      return nullptr;
    }
    const auto& accessor = a.accessors[p.indicesAccessor.value()];
    if (!accessor.bufferViewIndex.has_value()) {
      logSys->log(log_type_tapi_WARNING, false, L"Submesh has no index data, so skipped it");
      return nullptr;
    }
    if (accessor.type != fastgltf::AccessorType::Scalar) {
      logSys->log(log_type_tapi_WARNING, false,
                  L"Submesh's index buffer type is unsupported, so skipped it");
      return nullptr;
    }
    iCount = accessor.count;
  }

  if (p.type != fastgltf::PrimitiveType::Triangles) {
    logSys->log(log_type_tapi_WARNING, false,
                L"Submesh's primitive type isn't supported, so skipped it");
    return nullptr;
  }

  size_t attribFoundLocs[attribCount] = {};
  for (uint32_t attribIndx = 0; attribIndx < attribCount; attribIndx++) {
    auto attribLoc = p.attributes.find(attribNames[attribIndx]);
    if (attribLoc == p.attributes.end()) {
      logSys->log(log_type_tapi_WARNING, false, L"Submesh has no %s, so skipped it",
                  attribNames[attribIndx]);
      return nullptr;
    }
    attribFoundLocs[attribIndx] = attribLoc->second;
  }

  uint32_t vCount = UINT32_MAX;
  for (uint32_t attribIndx = 0; attribIndx < attribCount; attribIndx++) {
    const auto& accessor = a.accessors[attribFoundLocs[attribIndx]];
    if (!accessor.bufferViewIndex.has_value()) {
      logSys->log(log_type_tapi_WARNING, false, L"Submesh has no data of %s, so skipped it",
                  attribNames[attribIndx]);
      return nullptr;
    }
    if (accessor.type != attribTypes[attribIndx]) {
      logSys->log(log_type_tapi_WARNING, false,
                  L"Submesh has unsupported type of data in %s, so skipped it",
                  attribNames[attribIndx]);
      return nullptr;
    }
    if (vCount == UINT32_MAX) {
      vCount = accessor.count;
    } else if (vCount != accessor.count) {
      logSys->log(log_type_tapi_WARNING, false,
                  L"Submesh's vertex attribute aren't matching, so skipped it");
      return nullptr;
    }
  }

  void*  meshData = nullptr;
  rtMesh mesh     = rtMeshManager::allocateMesh(vCount, iCount, &meshData);
  meshes.push_back(mesh);

  for (uint32_t attribIndx = 0; attribIndx < attribCount; attribIndx++) {
    const auto& accessor   = a.accessors[attribFoundLocs[attribIndx]];
    const auto& view       = a.bufferViews[accessor.bufferViewIndex.value()];
    auto        dataOffset = accessor.byteOffset + view.byteOffset;
    uint32_t    gStride =
      (view.byteStride.has_value()) ? (view.byteStride.value()) : (attribElementSize[attribIndx]);
    const fastgltf::sources::Vector* buf =
      std::get_if<fastgltf::sources::Vector>(&a.buffers[view.bufferIndex].data);
    for (uint32_t vIndx = 0; vIndx < vCount; vIndx++) {
      memcpy((( char* )meshData) + (sizeof(vertex_rt) * vIndx),
             (( char* )buf->bytes.data()) + dataOffset + (gStride * vIndx),
             attribElementSize[attribIndx]);
    }
  }
  // Fill index buffer
  {
    const auto&                      accessor   = a.accessors[p.indicesAccessor.value()];
    const auto&                      view       = a.bufferViews[accessor.bufferViewIndex.value()];
    auto                             dataOffset = accessor.byteOffset + view.byteOffset;
    const fastgltf::sources::Vector* buf =
      std::get_if<fastgltf::sources::Vector>(&a.buffers[view.bufferIndex].data);
    uint32_t* indexBuffer = ( uint32_t* )((( uintptr_t )meshData) + (vCount * sizeof(rtVertex)));
    for (uint32_t iIndx = 0; iIndx < iCount; iIndx++) {
      uint32_t indice = UINT32_MAX;
      if (accessor.componentType == fastgltf::ComponentType::UnsignedShort) {
        uint16_t i_16 = UINT16_MAX;
        memcpy(&i_16, (( char* )buf->bytes.data()) + dataOffset + (sizeof(uint16_t) * iIndx),
               sizeof(uint16_t));
        indice = i_16;
      } else {
        memcpy(&indice, (( char* )buf->bytes.data()) + dataOffset + (sizeof(uint32_t) * iIndx),
               sizeof(uint32_t));
      }
      indexBuffer[iIndx] = indice;
    }
  }
}
#endif
struct rtResource** importFileUsingGLTF(const wchar_t* i_PATH, uint64_t* resourceCount) {
#ifdef FASTGLTF_VERSION
  // Parse the file
  fastgltf::Parser         parser;
  fastgltf::GltfDataBuffer data;
  std::filesystem::path    stdPath = i_PATH;
  data.loadFromFile(stdPath);
  auto gltf = parser.loadGLTF(&data, stdPath.parent_path(), fastgltf::Options::None);
  if (parser.getError() != fastgltf::Error::None) {
    gltf = parser.loadBinaryGLTF(&data, stdPath.parent_path());
  }
  if (parser.getError() != fastgltf::Error::None) {
    logSys->log(log_type_tapi_ERROR, true,
                L"GLTF file doesn't exist, couldn't be read or isn't a valid JSON document!");
    return nullptr;
  }
  if (gltf->parse() != fastgltf::Error::None) {
    logSys->log(log_type_tapi_ERROR, true,
                L"Either file or fastGLTF library doesn't follow the GLTF spec!");
    return nullptr;
  }
#ifdef _DEBUG
  if (gltf->validate() != fastgltf::Error::None) {
    logSys->log(log_type_tapi_ERROR, true, L"GLTF file validation has failed!");
    return nullptr;
  }
#endif

  std::unique_ptr<const fastgltf::Asset> parsedGLTF = gltf->getParsedAsset();

  std::vector<rtResource> resources;
  std::vector<rtMesh>     meshes;

  // Import meshes
  for (uint32_t mIndx = 0; mIndx < parsedGLTF->meshes.size(); mIndx++) {
    const auto& m = parsedGLTF->meshes[mIndx];
    logSys->log(log_type_tapi_STATUS, false, L"Processing mesh: %s_%u", m.name.c_str(), mIndx);
    for (uint32_t pIndx = 0; pIndx < m.primitives.size(); pIndx++) {
      rtMesh mesh = rtMeshManager::createDefaultMesh(*parsedGLTF, m.primitives[pIndx]);
      if (!mesh) {
        continue;
      }
      meshes.push_back(mesh);

      rtResourceDesc resourceDesc = {};
      resourceDesc.managerType    = rtMeshManager::managerType();
      resourceDesc.resourceHnd    = mesh;
      stringSys->createString(string_type_tapi_UTF8, ( void** )&resourceDesc.pathNameExt,
                              L"%v/%s_%u_%u", SOURCE_DIR L"Content/Meshes", m.name.c_str(), mIndx,
                              pIndx);

      resources.push_back(rtResourceManager::createResource(resourceDesc));
    }
  }

  // Import scenes
  for (uint32_t sIndx = 0; sIndx < parsedGLTF->scenes.size(); sIndx++) {
    const auto& gScene = parsedGLTF->scenes[sIndx];
    rtScene     scene  = rtSceneManager::createScene();
    rtSceneModifier::createEntitiesWithGLTF(scene, gScene, meshes.data());

    rtResourceDesc resourceDesc = {};
    resourceDesc.managerType    = rtSceneManager::managerType();
    resourceDesc.resourceHnd    = scene;
    stringSys->createString(string_type_tapi_UTF8, ( void** )&resourceDesc.pathNameExt, L"%v/%s_%u",
                            SOURCE_DIR L"Content/Scenes", gScene.name.c_str(), sIndx);

    resources.push_back(rtResourceManager::createResource(resourceDesc));
  }

  rtResource* finalList = new rtResource[resources.size()];
  memcpy(finalList, resources.data(), resources.size() * sizeof(rtResource));
  *resourceCount = resources.size();
  return finalList;
#endif
  return nullptr;
}