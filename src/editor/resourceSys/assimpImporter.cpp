#include <assert.h>
#include <vector>
#include <glm/glm.hpp>
// Assimp libraries to load Model
#include <assimp/postprocess.h>
#include <assimp/scene.h>
#include <assimp/Importer.hpp>

#include <logger_tapi.h>
#include <string_tapi.h>
#include <tgfx_structs.h>
#include <tgfx_forwarddeclarations.h>
#include <ecs_tapi.h>

#include "../editor_includes.h"
#include "resourceManager.h"
#include "mesh.h"
#include "../render_context/forwardMesh.h"
#include "scene.h"

#ifdef AI_ASSIMP_HPP_INC
rtMesh createForwardMesh(aiMesh* aMesh) {
  void*  meshData = nullptr;
  rtMesh mesh =
    rtForwardMeshManager::allocateMesh(aMesh->mNumVertices, aMesh->mNumFaces * 3, &meshData);

  rtForwardVertex* vertexBuffer = ( rtForwardVertex* )meshData;
  uint32_t*        indexBuffer =
    ( uint32_t* )((( uintptr_t )vertexBuffer) + (aMesh->mNumVertices * sizeof(rtForwardVertex)));

  for (uint32_t v = 0; v < aMesh->mNumVertices; v++) {
    vertexBuffer[v].pos = {aMesh->mVertices[v].x, aMesh->mVertices[v].y, aMesh->mVertices[v].z};
  }
  for (uint32_t v = 0; v < aMesh->mNumVertices; v++) {
    vertexBuffer[v].normal = {aMesh->mNormals[v].x, aMesh->mNormals[v].y, aMesh->mNormals[v].z};
  }
  for (uint32_t v = 0; v < aMesh->mNumVertices; v++) {
    if (aMesh->mTextureCoords[0]) {
      vertexBuffer[v].textCoord = {aMesh->mTextureCoords[0][v].x, aMesh->mTextureCoords[0][v].y};
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
  rtForwardMeshManager::forwardMesh_uploadFnc(mesh);
  return mesh;
}
#endif
#ifdef AI_ASSIMP_HPP_INC
void fillEntity(rtScene scene, entityHnd_ecstapi ntt, aiNode* node, rtMesh* const meshes) {
  compType_ecstapi  compType = {};
  defaultComponent_rt* comp     = ( defaultComponent_rt* )editorECS->get_component_byEntityHnd(
    ntt, defaultComponent_rt::getDefaultComponentTypeID(), &compType);
  assert(comp && "Default component isn't found!");
  comp->m_meshes.resize(node->mNumMeshes);
  for (uint32_t i = 0; i < node->mNumMeshes; i++) {
    comp->m_meshes[i] = meshes[node->mMeshes[i]];
  }
  for (uint8_t i = 0; i < 4; i++) {
    for (uint8_t j = 0; j < 4; j++) {
      comp->m_worldTransform[i][j] = node->mTransformation[i][j];
    }
  }
  comp->m_children.resize(node->mNumChildren);
  for (uint32_t i = 0; i < node->mNumChildren; i++) {
    comp->m_children[i] = rtSceneModifier::addDefaultEntity(scene);
    fillEntity(scene, comp->m_children[i], node->mChildren[i], meshes);
  }
}
void createEntitiesWithAssimp(rtScene scene, aiNode* rootNode,
                                                rtMesh* const meshes) {
  entityHnd_ecstapi rootNtt = rtSceneModifier::addDefaultEntity(scene);
  fillEntity(scene, rootNtt, rootNode, meshes);
}
#endif // ASSIMP

rtResource* importFileUsingAssimp(const wchar_t* i_PATH, uint64_t* resourceCount) {
  Assimp::Importer          import;
  const aiScene*            aScene              = nullptr;
  static constexpr uint32_t maxPathLen          = 2048;
  char                      i_PATH8[maxPathLen] = {};
  {
    stringSys->convertString(string_type_tapi_UTF16, i_PATH, string_type_tapi_UTF8,
                             ( void* )i_PATH8, maxPathLen - 1);
    aScene = import.ReadFile(i_PATH8, 0 | aiProcess_GenNormals | aiProcess_CalcTangentSpace |
                                        aiProcess_FindDegenerates | aiProcess_FlipUVs |
                                        aiProcess_Triangulate | aiProcess_GenUVCoords |
                                        aiProcess_FindInvalidData);

    // Check if scene reading errors!
    if (!aScene || aScene->mFlags & AI_SCENE_FLAGS_INCOMPLETE || !aScene->mRootNode) {
      printf("FAIL: Assimp couldn't load the file. Log: %s\n", import.GetErrorString());
      return nullptr;
    }
  }

  std::vector<rtResource> resources;
  std::vector<rtMesh>     meshes;
  for (uint32_t i = 0; i < aScene->mNumMeshes; i++) {
    rtMesh mesh = createForwardMesh(aScene->mMeshes[i]);
    if (!mesh) {
      continue;
    }
    meshes.push_back(mesh);

    rtResourceDesc resourceDesc = {};
    resourceDesc.managerType    = rtMeshManager::managerType();
    resourceDesc.resourceHnd    = mesh;
    std::string meshName        = SOURCE_DIR "Content/Meshes/";

    bool isThereNameCollision = false;
    // TODO: Check against name collision across meshes in the same Assimp Scene.

    if (aScene->mMeshes[i]->mName.length && !isThereNameCollision) {
      meshName += aScene->mMeshes[i]->mName.C_Str();
    } else {
      // TODO: Find the first aiNode in the scene that references this mesh
      //  then use entity's name as mesh name
    }

    resourceDesc.pathNameExt = meshName.c_str();
    resources.push_back(rtResourceManager::createResource(resourceDesc));
  }

  {
    rtScene scene = rtSceneManager::createScene();
    createEntitiesWithAssimp(scene, aScene->mRootNode, meshes.data());
    rtResourceDesc resourceDesc = {};
    resourceDesc.managerType    = rtSceneManager::managerType();
    resourceDesc.resourceHnd    = scene;
    std::string meshName        = SOURCE_DIR "Content/Scenes/";

    if (aScene->mRootNode->mName.length) {
      meshName += aScene->mRootNode->mName.C_Str();
    } else {
      meshName += aScene->GetShortFilename(i_PATH8);
    }
    resourceDesc.pathNameExt = meshName.c_str();

    resources.push_back(rtResourceManager::createResource(resourceDesc));
  }

  rtResource* finalList = new rtResource[resources.size()];
  memcpy(finalList, resources.data(), resources.size() * sizeof(rtResource));
  *resourceCount = resources.size();
  return finalList;
}