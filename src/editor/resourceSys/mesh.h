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
  static void   frame();

  static void                  initializeManager();
  typedef rtMesh               defaultResourceType;
  static rtResourceManagerType managerType();
} rtMeshManager;