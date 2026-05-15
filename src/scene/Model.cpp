#include "scene/Model.h"
#include "render/Texture.h"

#include <assimp/Importer.hpp>
#include <assimp/material.h>
#include <assimp/postprocess.h>

#include <glm/vec3.hpp>

#include <filesystem>
#include <iostream>
#include <limits>
#include <sstream>
#include <unordered_set>

namespace {

std::string normalizePathString(const std::filesystem::path& path)
{
    std::error_code ec;
    const std::filesystem::path canonicalPath = std::filesystem::weakly_canonical(path, ec);
    if (!ec) {
        return canonicalPath.generic_string();
    }
    return path.lexically_normal().generic_string();
}

std::string extractTexturePathToken(const std::string& rawPath)
{
    std::istringstream stream(rawPath);
    std::string token;
    std::string lastToken;
    while (stream >> token) {
        lastToken = token;
    }
    return lastToken.empty() ? rawPath : lastToken;
}

std::string makeEmbeddedTextureKey(const std::string& path)
{
    return "embedded:" + path;
}

std::string makeFileTextureKey(const std::string& normalizedPath)
{
    return "file:" + normalizedPath;
}

} // namespace

Model::Model(const std::string& modelPath, const std::string& fallbackDiffuseTexturePath)
{
    loadModel(modelPath);
    if (loaded_ && !fallbackDiffuseTexturePath.empty()) {
        const GLuint tid = loadTexture2DFromFile(fallbackDiffuseTexturePath);
        if (tid != 0) {
            TextureAsset tex{tid, "texture_diffuse", fallbackDiffuseTexturePath};
            loadedTextures_.push_back(tex);
            for (Mesh& mesh : meshes_) {
                mesh.attachTextureIfMissing(tex);
            }
        }
    }
}

Model::~Model()
{
    std::unordered_set<GLuint> deleted;
    for (const TextureAsset& texture : loadedTextures_) {
        if (texture.id != 0 && deleted.insert(texture.id).second) {
            glDeleteTextures(1, &texture.id);
        }
    }
}

void Model::drawGeometryOnly() const
{
    for (const Mesh& mesh : meshes_) {
        mesh.drawDirect();
    }
}

void Model::draw(const Shader& shader) const
{
    for (const Mesh& mesh : meshes_) {
        mesh.draw(shader);
    }
}

void Model::createVertexArraysForCurrentContext()
{
    for (Mesh& mesh : meshes_) {
        mesh.createVertexArrayForCurrentContext();
    }
}

void Model::releaseVertexArraysForCurrentContext()
{
    for (Mesh& mesh : meshes_) {
        mesh.releaseVertexArrayForCurrentContext();
    }
}

void Model::loadModel(const std::string& modelPath)
{
    Assimp::Importer importer;
    const aiScene* scene = importer.ReadFile(
        modelPath,
        aiProcess_Triangulate
            | aiProcess_CalcTangentSpace
            | aiProcess_GenSmoothNormals
            | aiProcess_JoinIdenticalVertices
            | aiProcess_FlipUVs
            | aiProcess_ImproveCacheLocality
            | aiProcess_FixInfacingNormals);

    if (scene == nullptr || (scene->mFlags & AI_SCENE_FLAGS_INCOMPLETE) != 0 || scene->mRootNode == nullptr) {
        std::cerr << "[Model] Failed to load model with Assimp.\n"
                  << "  Path: " << modelPath << '\n'
                  << "  Assimp error: " << importer.GetErrorString() << '\n';
        loaded_ = false;
        return;
    }

    directory_ = std::filesystem::path(modelPath).parent_path().string();
    processNode(scene->mRootNode, scene);
    loaded_ = !meshes_.empty();

    if (!loaded_) {
        std::cerr << "[Model] Model loaded but no mesh data was extracted.\n"
                  << "  Path: " << modelPath << '\n';
    } else {
        std::cerr << "[Model] Loaded model successfully.\n"
                  << "  Path: " << modelPath << '\n'
                  << "  Mesh count: " << meshes_.size() << '\n';
    }
}

void Model::processNode(aiNode* node, const aiScene* scene)
{
    for (unsigned int i = 0; i < node->mNumMeshes; ++i) {
        aiMesh* mesh = scene->mMeshes[node->mMeshes[i]];
        meshes_.push_back(processMesh(mesh, scene));
    }

    for (unsigned int i = 0; i < node->mNumChildren; ++i) {
        processNode(node->mChildren[i], scene);
    }
}

Mesh Model::processMesh(aiMesh* mesh, const aiScene* scene)
{
    std::vector<Vertex> vertices;
    std::vector<unsigned int> indices;
    std::vector<TextureAsset> textures;
    Material materialData;

    vertices.reserve(mesh->mNumVertices);
    indices.reserve(mesh->mNumFaces * 3);
    for (unsigned int i = 0; i < mesh->mNumVertices; ++i) {
        Vertex vertex;

        vertex.position = glm::vec3(mesh->mVertices[i].x, mesh->mVertices[i].y, mesh->mVertices[i].z);

        if (mesh->HasNormals()) {
            vertex.normal = glm::vec3(mesh->mNormals[i].x, mesh->mNormals[i].y, mesh->mNormals[i].z);
        } else {
            vertex.normal = glm::vec3(0.0f, 1.0f, 0.0f);
        }

        if (mesh->HasTangentsAndBitangents()) {
            vertex.tangent = glm::vec3(mesh->mTangents[i].x, mesh->mTangents[i].y, mesh->mTangents[i].z);
            vertex.bitangent = glm::vec3(mesh->mBitangents[i].x, mesh->mBitangents[i].y, mesh->mBitangents[i].z);
        } else {
            vertex.tangent = glm::vec3(1.0f, 0.0f, 0.0f);
            vertex.bitangent = glm::vec3(0.0f, 0.0f, 1.0f);
        }

        if (mesh->mTextureCoords[0] != nullptr) {
            vertex.texCoords = glm::vec2(mesh->mTextureCoords[0][i].x, mesh->mTextureCoords[0][i].y);
        } else {
            vertex.texCoords = glm::vec2(0.0f, 0.0f);
        }

        vertices.push_back(vertex);
    }

    for (unsigned int i = 0; i < mesh->mNumFaces; ++i) {
        const aiFace& face = mesh->mFaces[i];
        for (unsigned int j = 0; j < face.mNumIndices; ++j) {
            indices.push_back(face.mIndices[j]);
        }
    }

    if (mesh->mMaterialIndex < scene->mNumMaterials && scene->mMaterials[mesh->mMaterialIndex] != nullptr) {
        aiMaterial* material = scene->mMaterials[mesh->mMaterialIndex];
        aiColor3D diffuseColor(0.8f, 0.8f, 0.8f);
        if (material->Get(AI_MATKEY_COLOR_DIFFUSE, diffuseColor) == AI_SUCCESS) {
            materialData.diffuse = glm::vec3(diffuseColor.r, diffuseColor.g, diffuseColor.b);
        }

        std::vector<TextureAsset> diffuseTextures = loadMaterialTextures(material, aiTextureType_DIFFUSE, "texture_diffuse", scene);
        textures.insert(textures.end(), diffuseTextures.begin(), diffuseTextures.end());

        std::vector<TextureAsset> specularTextures = loadMaterialTextures(material, aiTextureType_SPECULAR, "texture_specular", scene);
        textures.insert(textures.end(), specularTextures.begin(), specularTextures.end());

        std::vector<TextureAsset> shininessTextures = loadMaterialTextures(material, aiTextureType_SHININESS, "texture_roughness", scene);
        textures.insert(textures.end(), shininessTextures.begin(), shininessTextures.end());

        std::vector<TextureAsset> normalTextures = loadMaterialTextures(material, aiTextureType_NORMALS, "texture_normal", scene);
        textures.insert(textures.end(), normalTextures.begin(), normalTextures.end());

        std::vector<TextureAsset> heightTextures = loadMaterialTextures(material, aiTextureType_HEIGHT, "texture_normal", scene);
        textures.insert(textures.end(), heightTextures.begin(), heightTextures.end());
    }

    return Mesh(std::move(vertices), std::move(indices), std::move(textures), materialData);
}

std::vector<TextureAsset> Model::loadMaterialTextures(aiMaterial* material, aiTextureType type, const std::string& typeName, const aiScene* scene)
{
    std::vector<TextureAsset> textures;

    const unsigned int count = material->GetTextureCount(type);
    for (unsigned int i = 0; i < count; ++i) {
        aiString aiPath;
        if (material->GetTexture(type, i, &aiPath) != AI_SUCCESS) {
            continue;
        }

        std::string rawPath = extractTexturePathToken(aiPath.C_Str());

        const aiTexture* embeddedTexture = scene ? scene->GetEmbeddedTexture(rawPath.c_str()) : nullptr;
        if (embeddedTexture != nullptr) {
            const std::string cacheKey = makeEmbeddedTextureKey(rawPath);
            auto cached = textureCache_.find(cacheKey);
            if (cached != textureCache_.end()) {
                textures.push_back(TextureAsset{cached->second, typeName, cacheKey});
                continue;
            }

            GLuint glId = 0;
            if (embeddedTexture->mHeight == 0) {
                glId = loadTexture2DFromMemory(
                    reinterpret_cast<const unsigned char*>(embeddedTexture->pcData),
                    static_cast<int>(embeddedTexture->mWidth),
                    true);
            } else {
                glId = createTexture2DFromRGBAPixels(
                    embeddedTexture->pcData,
                    static_cast<int>(embeddedTexture->mWidth),
                    static_cast<int>(embeddedTexture->mHeight),
                    true);
            }

            if (glId != 0) {
                textureCache_[cacheKey] = glId;
                TextureAsset texture{glId, typeName, cacheKey};
                textures.push_back(texture);
                loadedTextures_.push_back(texture);
            } else {
                std::cerr << "[Model] Embedded texture decode failed: " << rawPath << "\n";
            }
            continue;
        }

        if (rawPath.size() >= 2 && rawPath[0] == '*' && rawPath[1] >= '0' && rawPath[1] <= '9' && scene) {
            const int idx = std::stoi(rawPath.substr(1));
            if (idx >= 0 && idx < static_cast<int>(scene->mNumTextures) && scene->mTextures[idx] != nullptr) {
                const std::string cacheKey = makeEmbeddedTextureKey(rawPath);
                auto cached = textureCache_.find(cacheKey);
                if (cached != textureCache_.end()) {
                    textures.push_back(TextureAsset{cached->second, typeName, cacheKey});
                    continue;
                }

                const aiTexture* tex = scene->mTextures[idx];
                GLuint glId = 0;
                if (tex->mHeight == 0) {
                    glId = loadTexture2DFromMemory(
                        reinterpret_cast<const unsigned char*>(tex->pcData),
                        static_cast<int>(tex->mWidth),
                        true);
                } else {
                    glId = createTexture2DFromRGBAPixels(
                        tex->pcData,
                        static_cast<int>(tex->mWidth),
                        static_cast<int>(tex->mHeight),
                        true);
                }

                if (glId != 0) {
                    textureCache_[cacheKey] = glId;
                    TextureAsset texture{glId, typeName, cacheKey};
                    textures.push_back(texture);
                    loadedTextures_.push_back(texture);
                } else {
                    std::cerr << "[Model] Embedded texture decode failed: " << rawPath << "\n";
                }
                continue;
            }
        }

        std::filesystem::path resolvedPath(rawPath);
        if (!resolvedPath.is_absolute()) {
            resolvedPath = std::filesystem::path(directory_) / resolvedPath;
        }

        const std::string normalized = normalizePathString(resolvedPath);
        const std::string cacheKey = makeFileTextureKey(normalized);

        auto cached = textureCache_.find(cacheKey);
        if (cached != textureCache_.end()) {
            textures.push_back(TextureAsset{cached->second, typeName, normalized});
            continue;
        }

        TextureAsset texture;
        texture.id = loadTexture2DFromFile(normalized);
        texture.type = typeName;
        texture.path = normalized;
        if (texture.id != 0) {
            textureCache_[cacheKey] = texture.id;
            textures.push_back(texture);
            loadedTextures_.push_back(texture);
        } else {
            std::cerr << "[Model] Texture load failed: " << normalized << "\n";
        }
    }
    return textures;
}

void Model::worldBounds(const glm::mat4& modelMatrix, glm::vec3& outMin, glm::vec3& outMax) const
{
    glm::vec3 mn(std::numeric_limits<float>::max());
    glm::vec3 mx(std::numeric_limits<float>::lowest());
    for (const Mesh& mesh : meshes_) {
        mesh.accumulateWorldBounds(modelMatrix, mn, mx);
    }
    outMin = mn;
    outMax = mx;
}
