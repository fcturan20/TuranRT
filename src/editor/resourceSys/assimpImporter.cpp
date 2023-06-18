#include <assert.h>
#include <vector>
#include <glm/glm.hpp>
// Assimp libraries to load Model
#include <assimp/postprocess.h>
#include <assimp/scene.h>
#include <assimp/Importer.hpp>
// STB to load image
#include <stb_image.h>

#include <logger_tapi.h>
#include <string_tapi.h>
#include <tgfx_structs.h>
#include <tgfx_forwarddeclarations.h>
#include <tgfx_gpucontentmanager.h>
#include <ecs_tapi.h>

#include "../editor_includes.h"
#include "resourceManager.h"
#include "shaderEffect.h"
#include "mesh.h"
#include "../render_context/forwardMesh.h"
#include "../render_context/rendercontext.h"
#include "scene.h"
#include "surfaceMaterial.h"

#ifdef AI_ASSIMP_HPP_INC
rtMesh* ASSIMP_createForwardMesh(aiMesh* aMesh) {
  void*   meshData = nullptr;
  rtMesh* mesh     = forwardMM_allocateMesh(aMesh->mNumVertices, aMesh->mNumFaces * 3, &meshData);

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
  MM_uploadMeshes(1, &mesh);
  return mesh;
}

teSurfaceMaterialInstance* ASSIMP_createMaterial(const aiMaterial* aMaterial) {
  teSurfaceMaterialInstance* sei = SMM_createPhongMaterialInstance();

  // Diffuse texture
  {
    tgfx_texture*           diffTexture;
    tgfx_textureDescription diffuseDesc;
    aiString                diffPath;
    aiTextureType           diffTextTypes[] = {aiTextureType_BASE_COLOR, aiTextureType_DIFFUSE,
                                               aiTextureType_DIFFUSE_ROUGHNESS, aiTextureType_EMISSIVE,
                                               aiTextureType_SPECULAR};
    for (uint32_t i = 0; i < length_c(diffTextTypes); i++) {
      aiReturn result = aMaterial->GetTexture(diffTextTypes[i], 0, &diffPath);
      if (!diffPath.length || result != aiReturn_SUCCESS) {
        continue;
      }
      // Load texture
      int res[3] = {};
      unsigned char* data = stbi_load(diffPath.C_Str(), &res[0], &res[1], &res[2], 4);
      if (!data) {
        continue;
      }
      diffuseDesc.channelType = texture_channels_tgfx_RGBA8UB;
      diffuseDesc.dataOrder   = textureOrder_tgfx_SWIZZLE;
      diffuseDesc.dimension   = texture_dimensions_tgfx_2D;
      diffuseDesc.mipCount    = 1;
      diffuseDesc.permittedQueueCount = 64;
      diffuseDesc.permittedQueues     = allQueues;
      diffuseDesc.resolution          = {uint32_t(res[0]), uint32_t(res[1])};
      diffuseDesc.usage = textureUsageMask_tgfx_RANDOMACCESS | textureUsageMask_tgfx_RASTERSAMPLE;
      
      contentManager->createTexture(gpu, &diffuseDesc, &diffTexture);
      break;
    }

    SEM_setSEI_texture(sei, SSEM_getDiffuseInput(), diffTexture);
  }

  return sei;
}

void ASSIMP_fillEntity(struct rtScene* scene, tapi_ecs_entity* ntt, const aiScene* assimpScene,
                       const aiNode* node, struct rtMesh* const* meshes,
                       struct rtShaderEffectInstance* const* SEIs) {
  void*                compType = {};
  rtMeshComponent* comp     = ( rtMeshComponent* )editorECS->get_component_byEntityHnd(
    ntt, sceneManager->getMeshComponentTypeID(), &compType);
  assert(comp && "Default component isn't found!");

  comp->m_meshes    = new rtMesh*[node->mNumMeshes];
  comp->m_SEIs      = new rtShaderEffectInstance*[node->mNumMeshes];
  comp->m_meshCount = node->mNumMeshes;
  for (uint32_t i = 0; i < node->mNumMeshes; i++) {
    comp->m_meshes[i] = meshes[node->mMeshes[i]];
    comp->m_SEIs[i]   = SEIs[assimpScene->mMeshes[node->mMeshes[i]]->mMaterialIndex];
  }
  for (uint8_t i = 0; i < 4; i++) {
    for (uint8_t j = 0; j < 4; j++) {
      comp->m_worldTransform.v[(i * 4) + j] = node->mTransformation[i][j];
    }
  }

  comp->m_children = new tapi_ecs_entity*[node->mNumChildren];
  for (uint32_t i = 0; i < node->mNumChildren; i++) {
    comp->m_children[i] = sceneManager->addEntity(scene, sceneManager->getMeshEntityType());
    ASSIMP_fillEntity(scene, comp->m_children[i], assimpScene, node->mChildren[i], meshes, SEIs);
  }
}
void ASSIMP_createEntities(struct rtScene* scene, const aiScene* assimpScene,
                           const aiNode* rootNode, struct rtMesh* const* meshes,
                           struct rtShaderEffectInstance* const* SEIs) {
  tapi_ecs_entity* rootNtt = sceneManager->addEntity(scene, sceneManager->getMeshEntityType());
  ASSIMP_fillEntity(scene, rootNtt, assimpScene, rootNode, meshes, SEIs);
}

struct rtResource** importFileUsingAssimp(const wchar_t* i_PATH, uint64_t* resourceCount) {
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

  std::vector<struct rtResource*> resources;
  std::vector<struct rtMesh*>     meshes;
  for (uint32_t i = 0; i < aScene->mNumMeshes; i++) {
    rtMesh* mesh = ASSIMP_createForwardMesh(aScene->mMeshes[i]);
    if (!mesh) {
      logSys->log(log_type_tapi_WARNING, false, L"Failed to import a mesh");
      continue;
    }
    meshes.push_back(mesh);

    rtResourceDesc resourceDesc = {};
    resourceDesc.managerType    = MM_managerType();
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
    resources.push_back(RM_createResource(&resourceDesc));
  }

  // Only simple surface materials are imported for now
  std::vector<struct rtShaderEffectInstance*> SEIs;
  for (uint32_t mIndx = 0; mIndx < aScene->mNumMaterials; mIndx++) {
    const aiMaterial*       aMat = aScene->mMaterials[mIndx];
    rtShaderEffectInstance* sei  = ASSIMP_createMaterial(aMat);
    if (!sei) {
      logSys->log(log_type_tapi_WARNING, false, L"Failed to import a surface material");
      continue;
    }
    SEIs.push_back(sei);

    rtResourceDesc resourceDesc = {};
    resourceDesc.managerType    = ();
    resourceDesc.resourceHnd    = sei;
    std::string seiName         = SOURCE_DIR "Content/SurfaceMaterials/";

    bool isThereNameCollision = false;
    // TODO: Check against name collision across materials in the same Assimp Scene.

    if (aScene->mMaterials[mIndx]->GetName().length && !isThereNameCollision) {
      seiName += aScene->mMaterials[mIndx]->GetName().C_Str();
    } else {
      // TODO: Find the first aiNode in the scene that references this mesh
      //  then use entity's name as mesh name
    }

    resourceDesc.pathNameExt = seiName.c_str();
    resources.push_back(RM_createResource(&resourceDesc));
  }

  {
    struct rtScene* scene = sceneManager->createScene();
    ASSIMP_createEntities(scene, aScene, aScene->mRootNode, meshes.data(), SEIs.data());
    rtResourceDesc resourceDesc = {};
    resourceDesc.managerType    = sceneManager->getResourceManagerType();
    resourceDesc.resourceHnd    = scene;
    std::string meshName        = SOURCE_DIR "Content/Scenes/";

    if (aScene->mRootNode->mName.length) {
      meshName += aScene->mRootNode->mName.C_Str();
    } else {
      meshName += aScene->GetShortFilename(i_PATH8);
    }
    resourceDesc.pathNameExt = meshName.c_str();

    resources.push_back(RM_createResource(&resourceDesc));
  }

  struct rtResource** finalList = new rtResource*[resources.size()];
  memcpy(finalList, resources.data(), resources.size() * sizeof(struct rtResource*));
  *resourceCount = resources.size();
  return finalList;
}

#else
struct rtResource** importFileUsingAssimp(const wchar_t* i_PATH, uint64_t* resourceCount) {
  return nullptr;
}
#endif // AI_ASSIMP_HPP_INC