#pragma once
#ifdef __cplusplus
extern "C" {
#endif
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

// Create, upload, render & destroy vertex_rt vertex buffers.
// Rendering is done with instanced indexed indirect.
// 
  // To avoid unnecessary RAM->RAM copies, get either staging or wait queue memory
  // Vertex Data bytes: [0, sizeof(rtVertex) * vertexCount)
  // Index Data bytes: [sizeof(rtVertex) * vertexCount, (sizeof(rtVertex) * vertexCount) +
  // (indexCount * 4)]
struct rtMesh* forwardMM_allocateMesh(unsigned int vertexCount, unsigned int indexCount,
                                   void** meshData);

void                            forwardMM_initializeManager();
const struct rtMeshManagerType* forwardMM_managerType();

#ifdef __cplusplus
}
#endif