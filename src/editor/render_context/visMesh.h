#pragma once
#ifdef __cplusplus
extern "C" {
#endif
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

// To avoid unnecessary RAM->RAM copies, get either staging or wait queue memory
struct rtMesh* visMM_allocateMesh(unsigned int vertexCount, unsigned int indexCount,
                                  void** meshData);
// Uploads memory block of allocateMesh's meshData to GPU and validates rtMesh
// All meshData should be ready
unsigned char visMM_uploadMesh(struct rtMesh* mesh);
unsigned char visMM_destroyMesh(struct rtMesh* mesh);

// Compute is to render meshes with ray tracing
struct tgfx_commandBundle* visMM_renderMesh(unsigned int count, const struct MM_renderInfo* const infos);
void                  visMM_frame();
unsigned char         visMM_supportsMaterial(struct rtMaterial* mat);

void                            visMM_initializeManager();
const struct rtMeshManagerType* visMM_managerType();

#ifdef __cplusplus
}
#endif