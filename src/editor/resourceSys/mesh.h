#pragma once
// Include glm/glm.hpp & resourceManager.h before this

// Default vertex buffer layout
typedef struct vertex_rt {
  glm::vec3 pos;
  glm::vec3 normal;
  glm::vec2 textCoord;
} rtVertex;

typedef struct mesh_rt* rtMesh;
struct aiMesh;
typedef struct tgfx_commandbundle_obj* commandBundle_tgfxhnd;
// Create, upload, render & destroy vertex_rt vertex buffers.
// Rendering is done with instanced indexed indirect.
typedef struct meshManager_rt {
  // To avoid unnecessary RAM->RAM copies, get either staging or wait queue memory
  // Vertex Data bytes: [0, sizeof(rtVertex) * vertexCount)
  // Index Data bytes: [sizeof(rtVertex) * vertexCount, (sizeof(rtVertex) * vertexCount) +
  // (indexCount * 4)]
  static rtMesh allocateMesh(uint32_t vertexCount, uint32_t indexCount, void** meshData);
  // Uploads memory block of allocateMesh's meshData to GPU and validates rtMesh
  // All meshData should be ready
  static void uploadMesh(rtMesh mesh);
  // Assimp mesh importer: Allocates mesh and uploads it
  static rtMesh createDefaultMesh(aiMesh* mesh);
  struct renderInfo {
    rtMesh     mesh;
    glm::mat4* transform;
  };
  // Compute is to render meshes with ray tracing
  static void render(unsigned int count, renderInfo* const infos, commandBundle_tgfxhnd* raster,
                     commandBundle_tgfxhnd* compute = nullptr);
  static void                  frame();

  static void                  initializeManager();
  typedef rtMesh               defaultResourceType;
  static rtResourceManagerType managerType();
} rtMeshManager;