#pragma once
// Include mesh.h, tgfx_forwarddeclarations.h & tgfx_structs.h before this

/*
 * This is to render meshes with classic forward rendering
 * Limits:
    * ShaderFX should implement a "vec4 surface_shading()"
    * Should define loc = 0 in vec3 vertexColor, loc = 1 in vec2 textCoord
*/

// Default vertex buffer layout
typedef struct forwardVertex_rt {
  vec3_tgfx pos;
  vec2_tgfx textCoord;
  vec3_tgfx normal;
} rtForwardVertex;

struct aiMesh;
// Create, upload, render & destroy vertex_rt vertex buffers.
// Rendering is done with instanced indexed indirect.
typedef struct forwardMeshManager_rt {
  // To avoid unnecessary RAM->RAM copies, get either staging or wait queue memory
  // Vertex Data bytes: [0, sizeof(rtVertex) * vertexCount)
  // Index Data bytes: [sizeof(rtVertex) * vertexCount, (sizeof(rtVertex) * vertexCount) +
  // (indexCount * 4)]
  static rtMesh allocateMesh(unsigned int vertexCount, unsigned int indexCount, void** meshData);
  // Uploads memory block of allocateMesh's meshData to GPU and validates rtMesh
  // All meshData should be ready
  static unsigned char forwardMesh_uploadFnc(rtMesh mesh);
  static unsigned char forwardMesh_destroyFnc(rtMesh);

  // Compute is to render meshes with ray tracing
  static commandBundle_tgfxhnd forwardMesh_renderFnc(unsigned int                           count,
                                                     const rtMeshManager::renderInfo* const infos);
  static void                  forwardMesh_frameFnc();

  static void              initializeManager();
  static rtMeshManagerType managerType();
} rtForwardMeshManager;