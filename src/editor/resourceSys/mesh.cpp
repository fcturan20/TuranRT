#include <vector>

#include <string_tapi.h>
#include <filesys_tapi.h>
#include <logger_tapi.h>
#include <tgfx_core.h>
#include <tgfx_forwarddeclarations.h>
#include <tgfx_renderer.h>
#include <tgfx_gpucontentmanager.h>
#include <tgfx_structs.h>

#include "../render_context/rendercontext.h"
#include "resourceManager.h"
#include "../editor_includes.h"
#include "mesh.h"
static bool                    deserializeMesh(rtResourceDesc* desc) { return false; }
static bool                    isMeshValid(void* dataHnd) { return false; }
std::vector<rtMeshManagerType> Mmts;
struct meshManagerType_rt {
  rtMeshManager::managerDesc desc;
};
rtResourceManagerType meshManagerRMT = {};

rtResourceManagerType meshManager_rt::managerType() { return meshManagerRMT; }

struct meshRt_base {
  rtMeshManagerType type;
};
void* meshManager_rt::createMeshHandle(rtMeshManagerType Mmt) {
  meshRt_base* base = ( meshRt_base* )malloc(sizeof(meshRt_base) + Mmt->desc.meshStructSize);
  base->type        = Mmt;
  return ( void* )(uintptr_t(base) + sizeof(meshRt_base));
}
meshRt_base* accessBaseMesh(rtMesh m) {
  return reinterpret_cast<meshRt_base*>(reinterpret_cast<uintptr_t>(m) - sizeof(meshRt_base));
}

rtMesh meshManager_rt::allocateMesh(rtMeshManagerType Mmt, uint32_t vertexCount,
                                    uint32_t indexCount, void* extraInfo, void** meshData) {
  return Mmt->desc.allocateMeshFnc(vertexCount, indexCount, extraInfo, meshData);
}
unsigned char meshManager_rt::uploadMeshes(unsigned int count, rtMesh* meshes) {
  unsigned char isAnyFailed = 0, isAnySucceeded = 0;
  for (uint32_t i = 0; i < count; i++) {
    meshRt_base* base = accessBaseMesh(meshes[i]);
    if (base->type->desc.uploadMeshFnc(meshes[i])) {
      isAnySucceeded = true;
    } else {
      isAnyFailed = true;
    }
  }
  return (isAnySucceeded) ? (isAnyFailed + isAnySucceeded) : 0;
}
commandBundle_tgfxhnd* meshManager_rt::renderMeshes(unsigned int            count,
                                                    const renderInfo* const infos, unsigned int* cmdBndleCount) {
  std::vector<std::vector<rtMeshManager::renderInfo>> infoPerType(Mmts.size());
  for (uint32_t i = 0; i < count; i++) {
    auto base = accessBaseMesh(infos[i].mesh);
    for (uint32_t MmtIndx = 0; MmtIndx < Mmts.size(); MmtIndx++) {
      if (base->type == Mmts[MmtIndx]) {
        infoPerType[MmtIndx].push_back(infos[i]);
        break;
      }
    }
  }
  uint32_t validBundleCount = 0;
  for (uint32_t mmtIndx = 0; mmtIndx < infoPerType.size(); mmtIndx++) {
    rtMeshManagerType manager  = Mmts[mmtIndx];
    const auto&       infoList = infoPerType[mmtIndx];
    if (!infoList.size()) {
      continue;
    }
    validBundleCount++;
  }
  commandBundle_tgfxhnd* bundles = new commandBundle_tgfxhnd[validBundleCount];
  for (uint32_t mmtIndx = 0, bndleIndx = 0; mmtIndx < infoPerType.size(); mmtIndx++) {
    rtMeshManagerType manager  = Mmts[mmtIndx];
    const auto&       infoList = infoPerType[mmtIndx];
    if (!infoList.size()) {
      continue;
    }
    bundles[bndleIndx++] = manager->desc.renderMeshFnc(infoList.size(), infoList.data());
  }
  *cmdBndleCount = validBundleCount;
  return bundles;
}
unsigned char meshManager_rt::destroyMeshes(unsigned int count, rtMesh* meshes) {
  unsigned char isAnyFailed = 0, isAnySucceeded = 0;
  for (uint32_t i = 0; i < count; i++) {
    meshRt_base* base = accessBaseMesh(meshes[i]);
    if (base->type->desc.destroyMeshFnc(meshes[i])) {
      isAnySucceeded = true;
    } else {
      isAnyFailed = true;
    }
  }
  return (isAnySucceeded) ? (isAnyFailed + isAnySucceeded) : 0;
}
void meshManager_rt::frame() {
  for (rtMeshManagerType Mmt : Mmts) {
    Mmt->desc.frameFnc();
  }
}

rtMeshManagerType meshManager_rt::registerManager(managerDesc desc) {
  rtMeshManagerType Mmt = new meshManagerType_rt;
  Mmt->desc             = desc;
  Mmts.push_back(Mmt);
  return Mmt;
}
void meshManager_rt::initializeManager() {
  rtResourceManager::managerDesc desc;
  desc.managerName       = "Mesh Resource Manager";
  desc.managerVer        = MAKE_PLUGIN_VERSION_TAPI(0, 0, 0);
  desc.deserialize       = deserializeMesh;
  desc.validate          = isMeshValid;
  meshManagerRMT    = rtResourceManager::registerManager(desc);
}