#pragma once
#include <glm/glm.hpp>
#include <editor/editor_includes.h>

struct tri_mesh {
    unsigned int v_count = 0, i_count = 0;
    glm::vec3* positions, *normals;
    unsigned int* indexbuffer;
};

struct tri_model{
    unsigned int meshcount;
    tri_mesh* meshes;
};


struct file_importer{
    //Import model from generic file formats with assimp
	static tri_model* ImportModel_withAssimp(const char* PATH);
    //Import model from custom binary flatbuffers file format of the project
    static tri_model* Import_CompiledModel(const char* PATH);
};


struct file_saver {
    static bool SaveModel_toCompileFile(tri_model* model, bool isBinary);
};