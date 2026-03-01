#include "Model3D.hpp"
#include <map>

namespace gps{

	void Model3D::LoadModel(std::string fileName){

		std::string basePath = fileName.substr(0, fileName.find_last_of('/')) + "/";
		ReadOBJ(fileName, basePath);
	}

	void Model3D::LoadModel(std::string fileName, std::string basePath){

		ReadOBJ(fileName, basePath);
	}

	void Model3D::Draw(gps::Shader shaderProgram){

		for (int i = 0; i < meshes.size(); i++)
			meshes[i].Draw(shaderProgram);
	}

	void Model3D::ReadOBJ(std::string fileName, std::string basePath){

		std::cout << "Loading : " << fileName << std::endl;
		tinyobj::attrib_t attrib;
		std::vector<tinyobj::shape_t> shapes;
		std::vector<tinyobj::material_t> materials;

		std::string err;
		bool ret = tinyobj::LoadObj(&attrib, &shapes, &materials, &err, fileName.c_str(), basePath.c_str(), GL_TRUE);

		if (!err.empty()){
			std::cerr << err << std::endl;
		}

		if (!ret){
			exit(1);
		}

		std::cout << "# of shapes    : " << shapes.size() << std::endl;
		std::cout << "# of materials : " << materials.size() << std::endl;

		for (size_t s = 0; s < shapes.size(); s++){

			std::map<int, std::vector<tinyobj::index_t>> materialGroups;

			size_t index_offset = 0;
			for (size_t f = 0; f < shapes[s].mesh.num_face_vertices.size(); f++){
				int fv = shapes[s].mesh.num_face_vertices[f];
				int materialId = shapes[s].mesh.material_ids[f];

				for (size_t v = 0; v < fv; v++){
					tinyobj::index_t idx = shapes[s].mesh.indices[index_offset + v];
					materialGroups[materialId].push_back(idx);
				}

				index_offset += fv;
			}

			for (auto& group : materialGroups){
				int materialId = group.first;
				std::vector<tinyobj::index_t>& groupIndices = group.second;

				// Print Material Info
				/*std::cout << "\n[Debug] Processing Material ID: " << materialId;
				if (materialId != -1 && materialId < materials.size()){
					std::cout << " | Name: " << materials[materialId].name;
				}
				else{
					std::cout << " | Name: (None/Default)";
				}
				std::cout << std::endl;*/

				std::vector<gps::Vertex> vertices;
				std::vector<GLuint> indices;
				std::vector<gps::Texture> textures;

				for (size_t i = 0; i < groupIndices.size(); i++){
					tinyobj::index_t idx = groupIndices[i];

					float vx = attrib.vertices[3 * idx.vertex_index + 0];
					float vy = attrib.vertices[3 * idx.vertex_index + 1];
					float vz = attrib.vertices[3 * idx.vertex_index + 2];
					float nx = attrib.normals[3 * idx.normal_index + 0];
					float ny = attrib.normals[3 * idx.normal_index + 1];
					float nz = attrib.normals[3 * idx.normal_index + 2];
					float tx = 0.0f;
					float ty = 0.0f;

					if (idx.texcoord_index != -1){
						tx = attrib.texcoords[2 * idx.texcoord_index + 0];
						ty = attrib.texcoords[2 * idx.texcoord_index + 1];
					}

					glm::vec3 vertexPosition(vx, vy, vz);
					glm::vec3 vertexNormal(nx, ny, nz);
					glm::vec2 vertexTexCoords(tx, ty);

					gps::Vertex currentVertex;
					currentVertex.Position = vertexPosition;
					currentVertex.Normal = vertexNormal;
					currentVertex.TexCoords = vertexTexCoords;

					vertices.push_back(currentVertex);
					indices.push_back((GLuint)i);
				}

				gps::Material currentMaterial;
				// Default fallback color
				currentMaterial.diffuse = glm::vec3(0.6f, 0.6f, 0.6f);

				if (materialId != -1 && materialId < materials.size()){
					const auto& mat = materials[materialId];

					//std::cout << "\n[Material Debug] Name: " << mat.name << " (ID: " << materialId << ")" << std::endl;

					// 1. Load Solid Colors
					currentMaterial.diffuse = glm::vec3(mat.diffuse[0], mat.diffuse[1], mat.diffuse[2]);
					//std::cout << "   -> Base Diffuse Color: (" << mat.diffuse[0] << ", " << mat.diffuse[1] << ", " << mat.diffuse[2] << ")" << std::endl;

					bool hasDiffuseTexture = !mat.diffuse_texname.empty();

					// 2. Load Diffuse Map 
					if (hasDiffuseTexture){
						//std::cout << "   -> Loading Diffuse Texture: " << mat.diffuse_texname << std::endl;
						textures.push_back(LoadTexture(basePath + mat.diffuse_texname, "diffuseTexture"));
					}
					else{
						//std::cout << "   -> No diffuse texture - using solid color" << std::endl;
					}

					// 3. Load Specular Map
					if (!mat.specular_texname.empty()){
						std::cout << "   -> Loading Specular Texture: " << mat.specular_texname << std::endl;
						textures.push_back(LoadTexture(basePath + mat.specular_texname, "specularTexture"));
					}

					// 4. Load Roughness
					if (!mat.specular_highlight_texname.empty()){
						//std::cout << "   -> Loading Roughness (map_Ns): " << mat.specular_highlight_texname << std::endl;
						textures.push_back(LoadTexture(basePath + mat.specular_highlight_texname, "specularTexture"));
					}

					// 5. Load Normal/Bump Map
					if (!mat.bump_texname.empty()){
						//std::cout << "   -> Loading Normal/Bump Map: " << mat.bump_texname << std::endl;
						textures.push_back(LoadTexture(basePath + mat.bump_texname, "normalTexture"));
					}

					// 6. Load Roughness Map (if stored separately)
					if (!mat.roughness_texname.empty()){
						//std::cout << "   -> Loading Roughness Texture: " << mat.roughness_texname << std::endl;
						textures.push_back(LoadTexture(basePath + mat.roughness_texname, "roughnessTexture"));
					}

					// 7. Load Metalness Map
					if (!mat.metallic_texname.empty()){
						//std::cout << "   -> Loading Metalness (map_refl): " << mat.metallic_texname << std::endl;
						textures.push_back(LoadTexture(basePath + mat.metallic_texname, "metalnessTexture"));
					}

					// 8. Alternative: Check unknown_parameter map for map_refl if metallic_texname is empty
					for (const auto& param : mat.unknown_parameter){
						if (param.first == "map_refl"){
							//std::cout << "   -> Loading Reflection Map (from unknown_parameter): " << param.second << std::endl;
							textures.push_back(LoadTexture(basePath + param.second, "metalnessTexture"));
						}
					}

					if (textures.empty()){
						//std::cout << "   [!] No textures found. This mesh will use solid color: ("
						//	<< currentMaterial.diffuse.r << ", "
						//	<< currentMaterial.diffuse.g << ", "
						//	<< currentMaterial.diffuse.b << ")" << std::endl;
					}
					else{
						//std::cout << "   => Success: " << textures.size() << " texture(s) attached." << std::endl;
					}
				}
				meshes.push_back(gps::Mesh(vertices, indices, textures, currentMaterial.diffuse));
				
			}
		}
	}

	gps::Texture Model3D::LoadTexture(std::string path, std::string type){
		for (int i = 0; i < loadedTextures.size(); i++){
			if (loadedTextures[i].path == path){
				return loadedTextures[i];
			}
		}

		gps::Texture currentTexture;
		currentTexture.id = ReadTextureFromFile(path.c_str());
		currentTexture.type = std::string(type);
		currentTexture.path = path;

		loadedTextures.push_back(currentTexture);

		return currentTexture;
	}

	GLuint Model3D::ReadTextureFromFile(const char* file_name){

		int x, y, n;
		int force_channels = 4;
		unsigned char* image_data = stbi_load(file_name, &x, &y, &n, force_channels);

		if (!image_data){
			fprintf(stderr, "ERROR: could not load %s\n", file_name);
			return false;
		}
		if ((x & (x - 1)) != 0 || (y & (y - 1)) != 0){
			fprintf(
				stderr, "WARNING: texture %s is not power-of-2 dimensions\n", file_name
			);
		}

		int width_in_bytes = x * 4;
		unsigned char* top = NULL;
		unsigned char* bottom = NULL;
		unsigned char temp = 0;
		int half_height = y / 2;

		for (int row = 0; row < half_height; row++){

			top = image_data + row * width_in_bytes;
			bottom = image_data + (y - row - 1) * width_in_bytes;

			for (int col = 0; col < width_in_bytes; col++){

				temp = *top;
				*top = *bottom;
				*bottom = temp;
				top++;
				bottom++;
			}
		}

		GLuint textureID;
		glGenTextures(1, &textureID);
		glBindTexture(GL_TEXTURE_2D, textureID);
		glTexImage2D(
			GL_TEXTURE_2D,
			0,
			GL_SRGB,
			x,
			y,
			0,
			GL_RGBA,
			GL_UNSIGNED_BYTE,
			image_data
		);
		glGenerateMipmap(GL_TEXTURE_2D);

		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		glBindTexture(GL_TEXTURE_2D, 0);

		return textureID;
	}

	Model3D::~Model3D(){

		for (size_t i = 0; i < loadedTextures.size(); i++){

			glDeleteTextures(1, &loadedTextures.at(i).id);
		}

		for (size_t i = 0; i < meshes.size(); i++){

			GLuint VBO = meshes.at(i).getBuffers().VBO;
			GLuint EBO = meshes.at(i).getBuffers().EBO;
			GLuint VAO = meshes.at(i).getBuffers().VAO;
			glDeleteBuffers(1, &VBO);
			glDeleteBuffers(1, &EBO);
			glDeleteVertexArrays(1, &VAO);
		}
	}
}