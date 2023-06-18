#pragma once
#ifdef __cplusplus
extern "C" {
#endif
// Include glm/glm.hpp & resourceManager.h before this

////////// USER INTERFACE

struct MM_renderInfo {
  struct rtMesh*                 mesh;
  struct rtShaderEffectInstance* sei;
  // World matrix of each mesh instance
  unsigned int   transformCount;
  struct rtMat4* transforms;
};

struct rtMesh* MM_allocateMesh(const struct rtMeshManagerType* Mmt, uint32_t vertexCount,
                               uint32_t indexCount, void* extraInfo, void** meshData);
unsigned char  MM_uploadMeshes(unsigned int count, struct rtMesh** meshes);
struct tgfx_commandBundle** MM_renderMeshes(unsigned int                      count,
                                            const struct MM_renderInfo* const infos,
                                            unsigned int*                     cmdBndleCount);
unsigned char               MM_destroyMeshes(unsigned int count, struct rtMesh** meshes);
void                        MM_frame();

///////////////////////////

///////////// Mesh Manager Type (Mmt) definitions (Only for Mmt implementors)

// Extra info is manager specific data input (like vertex attribute info in visibilityMesh)
typedef struct MM_managerDesc {
  const char* managerName;
  uint32_t    managerVer;
  uint32_t    meshStructSize;
  struct rtMesh* (*allocateMeshFnc)(uint32_t vertexCount, uint32_t indexCount, void* extraInfo,
                                    void** meshData);
  unsigned char (*uploadMeshFnc)(struct rtMesh* mesh);
  unsigned char (*destroyMeshFnc)(struct rtMesh* mesh);
  struct tgfx_commandBundle* (*renderMeshFnc)(unsigned int                      count,
                                              const struct MM_renderInfo* const infos);
  void (*frameFnc)();
  unsigned char (*supportsSEFnc)(struct rtSurfaceShaderEffect* mat);
} MM_managerDesc;
// Only for Mmt implementors
const struct rtMeshManagerType* MM_registerManager(MM_managerDesc desc);
// Only for Mmt implementors
void* MM_createMeshHandle(const struct rtMeshManagerType* managerType);

////////////////////////////

// Resource Manager Type (Rmt) implementation
const struct rtResourceManagerType* MM_managerType();
void                                initializeMeshManager();

#ifdef __cplusplus
}
#endif