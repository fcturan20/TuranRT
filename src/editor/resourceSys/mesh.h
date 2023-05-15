#pragma once
// Include glm/glm.hpp & resourceManager.h before this

typedef struct mesh_rt*            rtMesh;
typedef struct meshManagerType_rt* rtMeshManagerType;
typedef struct meshManager_rt {
  ////////// USER INTERFACE

  struct renderInfo {
    rtMesh mesh;
    struct mat4_rt* transform;
    struct shaderEffectInstance_rt* sei;
  };

  static rtMesh allocateMesh(rtMeshManagerType Mmt, uint32_t vertexCount, uint32_t indexCount,
                             void* extraInfo, void** meshData);
  static unsigned char          uploadMeshes(unsigned int count, rtMesh* meshes);
  static commandBundle_tgfxhnd* renderMeshes(unsigned int count, const renderInfo* const infos,
                                             unsigned int* cmdBndleCount);
  static unsigned char          destroyMeshes(unsigned int count, rtMesh* meshes);
  static void                   frame();

  ///////////////////////////

  ///////////// Mesh Manager Type (Mmt) definitions (Only for Mmt implementors)

  // Extra info is manager specific data input (like vertex attribute info in visibilityMesh)
  struct managerDesc {
    const char* managerName;
    uint32_t    managerVer;
    uint32_t    meshStructSize;
    rtMesh (*allocateMeshFnc)(uint32_t vertexCount, uint32_t indexCount, void* extraInfo,
                              void** meshData);
    unsigned char (*uploadMeshFnc)(rtMesh mesh);
    unsigned char (*destroyMeshFnc)(rtMesh mesh);
    commandBundle_tgfxhnd (*renderMeshFnc)(unsigned int count, const renderInfo* const infos);
    void (*frameFnc)();
    unsigned char (*supportsMaterialFnc)(struct surfaceShaderEffect_rt* mat);
  };
  // Only for Mmt implementors
  static rtMeshManagerType registerManager(managerDesc desc);
  // Only for Mmt implementors
  static void* createMeshHandle(rtMeshManagerType managerType);

  ////////////////////////////

  // Resource Manager Type (Rmt) implementation
  typedef rtMesh               defaultResourceType;
  static rtResourceManagerType managerType();
  static void                  initializeManager();
} rtMeshManager;