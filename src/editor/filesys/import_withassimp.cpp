#include "importer.h"


//Assimp libraries to load Model
#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>


tri_model* file_importer::ImportModel_withAssimp(const char* PATH){
	Assimp::Importer import;
	const aiScene* Scene = nullptr;
	{
		Scene = import.ReadFile(PATH, aiProcess_GenNormals | aiProcess_CalcTangentSpace | aiProcess_FlipUVs | aiProcess_Triangulate | aiProcess_FindInvalidData);

		//Check if scene reading errors!
		if (!Scene || Scene->mFlags & AI_SCENE_FLAGS_INCOMPLETE || !Scene->mRootNode) {
			printer_editor(result_editor::FAIL, ("Failed on Loading Mesh with Assimp; " + std::string(import.GetErrorString())).c_str());
		}

		if (Scene->mNumMeshes == 0) {
			printer_editor(result_editor::FAIL, "Failed because there is no mesh in loaded scene!");
		}
	}
        
	tri_model* model = new tri_model;
	model->meshes = new tri_mesh[Scene->mNumMeshes];
	//Importing process
	for (unsigned int i = 0; i < Scene->mNumMeshes; i++) {
		model->meshes[i].v_count = Scene->mMeshes[i]->mNumVertices;


		model->meshes[i].positions = new glm::vec3[model->meshes[i].v_count];
		memcpy(model->meshes->positions, Scene->mMeshes[i]->mVertices, sizeof(glm::vec3) * model->meshes[i].v_count);

		model->meshes[i].normals = new glm::vec3[model->meshes[i].v_count];
		memcpy(model->meshes->normals, Scene->mMeshes[i]->mNormals, sizeof(glm::vec3) * model->meshes[i].v_count);
	}

	return model;
}