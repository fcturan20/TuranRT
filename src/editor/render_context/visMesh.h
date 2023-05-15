#pragma once
// Include mesh.h & tgfx_structs.h before this

/*
 * This is to render meshes with visibility buffer
 * http://filmicworlds.com/blog/visibility-buffer-rendering-with-material-graphs/
 1) Vertex positions of all meshes are stored in single buffer.
 2) All other attribute datas're stored in a different buffer.
 3) All surfaceMaterials're run as full-quads, checking if texel's materialID is matching shader's
 materialID.
 4) Because surfaceMaterials may want to access different attributes, Mmt should compile
 its variation of accessing such attributes. For example; material may want to access textCoord_1
 for a double-layer affect. Shader will only used get_textCoord1() in its code. Implementing
 get_textCoord1() is Mmt's job.
 */

typedef struct visibilityMeshManager_rt {
  // To avoid unnecessary RAM->RAM copies, get either staging or wait queue memory
  static rtMesh allocateMesh(unsigned int vertexCount, unsigned int indexCount, void** meshData);
  // Uploads memory block of allocateMesh's meshData to GPU and validates rtMesh
  // All meshData should be ready
  static unsigned char uploadMesh(rtMesh mesh);
  static unsigned char destroyMesh(rtMesh);

  // Compute is to render meshes with ray tracing
  static commandBundle_tgfxhnd renderMesh(unsigned int                           count,
                                          const rtMeshManager::renderInfo* const infos);
  static void                  frame();
  static unsigned char      supportsMaterial(rtMaterial mat);

  static void              initializeManager();
  static rtMeshManagerType managerType();
} rtVisibilityMeshManager;