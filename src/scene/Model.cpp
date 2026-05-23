#include "scene/Model.h"
#include "render/Texture.h"

#include <GLFW/glfw3.h>

#include <assimp/Importer.hpp>
#include <assimp/ProgressHandler.hpp>
#include <assimp/material.h>
#include <assimp/postprocess.h>

#include <algorithm>
#include <cctype>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <limits>
#include <sstream>
#include <string>
#include <unordered_set>

namespace {

glm::mat4 toGlmMat4(const aiMatrix4x4& m)
{
    return glm::mat4(
        m.a1, m.b1, m.c1, m.d1,
        m.a2, m.b2, m.c2, m.d2,
        m.a3, m.b3, m.c3, m.d3,
        m.a4, m.b4, m.c4, m.d4);
}

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

std::string makeEmbeddedTextureKey(const aiTexture* texture)
{
    return "embedded_ptr:" + std::to_string(reinterpret_cast<std::uintptr_t>(texture));
}

std::string makeFileTextureKey(const std::string& normalizedPath)
{
    return "file:" + normalizedPath;
}

glm::vec3 normalizeOrFallback(const glm::vec3& value, const glm::vec3& fallback)
{
    const float len2 = glm::dot(value, value);
    if (len2 <= 1e-12f) {
        return fallback;
    }
    return value * (1.0f / std::sqrt(len2));
}

GLuint uploadEmbeddedTexture(const aiTexture* embeddedTexture, const std::string& label, bool flipV)
{
    if (embeddedTexture == nullptr) {
        return 0;
    }

    if (embeddedTexture->mHeight == 0) {
        // GLB 内嵌 PNG/JPEG 等压缩字节流：必须先�?stb 从内存解码�?
        return loadTexture2DFromMemory(
            reinterpret_cast<const unsigned char*>(embeddedTexture->pcData),
            static_cast<int>(embeddedTexture->mWidth),
            flipV);
    }

    // GLB/Assimp 已解压的 aiTexel 原始像素：不再走 stb，直接上传到 OpenGL�?
    // aiTexel 在内存中�?BGRA 排列，Texture.cpp 使用 GL_BGRA 正确解释�?
    const GLuint id = createTexture2DFromRGBAPixels(
        embeddedTexture->pcData,
        static_cast<int>(embeddedTexture->mWidth),
        static_cast<int>(embeddedTexture->mHeight),
        true);
    if (id == 0) {
        std::cerr << "[Model] Embedded raw texture upload failed: " << label << "\n";
    }
    return id;
}

std::string trimAscii(const std::string& value)
{
    const auto begin = value.find_first_not_of(" \t\r\n");
    if (begin == std::string::npos) {
        return {};
    }
    const auto end = value.find_last_not_of(" \t\r\n");
    return value.substr(begin, end - begin + 1);
}

unsigned countMeshesUnderNode(const aiNode* node)
{
    unsigned count = node->mNumMeshes;
    for (unsigned i = 0; i < node->mNumChildren; ++i) {
        count += countMeshesUnderNode(node->mChildren[i]);
    }
    return count;
}

class AssimpFileProgress final : public Assimp::ProgressHandler {
public:
    explicit AssimpFileProgress(Model::LoadProgressCallback cb)
        : cb_(std::move(cb))
    {
    }

    bool Update(float percentage) override
    {
        if (!cb_) {
            return true;
        }
        if (percentage < 0.0f) {
            cb_(0.12f, "Assimp: processing...");
            return true;
        }
        const float p = std::clamp(percentage, 0.0f, 1.0f);
        cb_(0.05f + 0.28f * p, "Assimp: reading / parsing file...");
        return true;
    }

private:
    Model::LoadProgressCallback cb_;
};

} // namespace

Model::Model(const std::string& modelPath, LoadProgressCallback onProgress)
    : progressCb_(std::move(onProgress))
{
    loadModel(modelPath);
    if (loaded_) {
        emitProgress(1.0f, "Model ready.");
    }
}

Model::~Model()
{
    if (glfwGetCurrentContext() == nullptr) {
        return;
    }

    std::unordered_set<GLuint> deleted;
    for (const TextureAsset& texture : loadedTextures_) {
        if (texture.id != 0 && deleted.insert(texture.id).second) {
            glDeleteTextures(1, &texture.id);
        }
    }
}

void Model::emitProgress(float normalized, const char* status)
{
    if (!progressCb_) {
        return;
    }
    progressCb_(std::clamp(normalized, 0.0f, 1.0f), status ? status : "");
}

void Model::drawGeometryOnly() const
{
    for (const Mesh& mesh : meshes_) {
        if (!mesh.isTransparent()) {
            mesh.drawDirect();
        }
    }
}

void Model::draw(const Shader& shader) const
{
    drawOpaque(shader);

    for (const Mesh& mesh : meshes_) {
        if (mesh.isTransparent()) {
            mesh.draw(shader);
        }
    }
}

void Model::drawOpaque(const Shader& shader) const
{
    for (const Mesh& mesh : meshes_) {
        if (!mesh.isTransparent()) {
            mesh.draw(shader);
        }
    }
}

void Model::appendTransparentDraws(const glm::mat4& modelMatrix, const glm::vec3& viewPosition,
                                   std::vector<TransparentMeshDraw>& out) const
{
    for (const Mesh& mesh : meshes_) {
        if (!mesh.isTransparent() || !mesh.hasLocalBounds()) {
            continue;
        }

        const glm::vec3 localCenter = 0.5f * (mesh.localBoundsMin() + mesh.localBoundsMax());
        const glm::vec3 worldCenter = glm::vec3(modelMatrix * glm::vec4(localCenter, 1.0f));
        const glm::vec3 delta = worldCenter - viewPosition;
        out.push_back({&mesh, modelMatrix, glm::dot(delta, delta)});
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
    emitProgress(0.0f, "Starting model load...");
    loadMaterialOpacityOverrides(modelPath);
    std::string extension = std::filesystem::path(modelPath).extension().string();
    std::transform(extension.begin(), extension.end(), extension.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    flipTexturesVertically_ = extension != ".glb" && extension != ".gltf";

    Assimp::Importer importer;
    AssimpFileProgress assimpProgress(progressCb_);
    if (progressCb_) {
        importer.SetProgressHandler(&assimpProgress);
    }

    unsigned int importFlags = aiProcess_Triangulate
        | aiProcess_CalcTangentSpace
        | aiProcess_GenSmoothNormals
        | aiProcess_ImproveCacheLocality;
    if (flipTexturesVertically_) {
        importFlags |= aiProcess_JoinIdenticalVertices
            | aiProcess_FixInfacingNormals;
    }

    emitProgress(0.02f, "Reading model file (Assimp)...");
    const aiScene* scene = importer.ReadFile(modelPath, importFlags);

    importer.SetProgressHandler(nullptr);

    if (scene == nullptr || (scene->mFlags & AI_SCENE_FLAGS_INCOMPLETE) != 0 || scene->mRootNode == nullptr) {
        std::cerr << "[Model] Failed to load model with Assimp.\n"
                  << "  Path: " << modelPath << '\n'
                  << "  Assimp error: " << importer.GetErrorString() << '\n';
        loaded_ = false;
        emitProgress(0.0f, "Load failed.");
        return;
    }

    directory_ = std::filesystem::path(modelPath).parent_path().string();
    meshDone_ = 0;
    meshTotal_ = std::max(1u, countMeshesUnderNode(scene->mRootNode));
    emitProgress(0.36f, "Building GPU meshes...");

    processNode(scene->mRootNode, scene, glm::mat4(1.0f));
    loaded_ = !meshes_.empty();

    if (!loaded_) {
        std::cerr << "[Model] Model loaded but no mesh data was extracted.\n"
                  << "  Path: " << modelPath << '\n';
        emitProgress(0.0f, "No mesh data extracted.");
    } else {
        std::cerr << "[Model] Loaded model successfully.\n"
                  << "  Path: " << modelPath << '\n'
                  << "  Mesh count: " << meshes_.size() << '\n';
        emitProgress(0.85f, "Geometry upload complete.");
    }
}

void Model::loadMaterialOpacityOverrides(const std::string& modelPath)
{
    materialOpacityOverrides_.clear();

    const std::filesystem::path mtlPath = std::filesystem::path(modelPath).replace_extension(".mtl");
    std::ifstream file(mtlPath);
    if (!file.is_open()) {
        return;
    }

    std::string currentMaterial;
    std::string line;
    while (std::getline(file, line)) {
        line = trimAscii(line);
        if (line.empty() || line[0] == '#') {
            continue;
        }

        std::istringstream stream(line);
        std::string key;
        stream >> key;
        if (key == "newmtl") {
            std::string name;
            std::getline(stream, name);
            currentMaterial = trimAscii(name);
            continue;
        }

        if (currentMaterial.empty()) {
            continue;
        }

        float value = 1.0f;
        if (key == "d" && (stream >> value)) {
            const float opacity = std::clamp(value, 0.0f, 1.0f);
            materialOpacityOverrides_[currentMaterial] = opacity;
            if (opacity < 0.995f) {
                std::cerr << "[Model] MTL opacity: " << currentMaterial << " d=" << opacity << '\n';
            }
        } else if (key == "Tr" && (stream >> value)) {
            const float opacity = std::clamp(1.0f - value, 0.0f, 1.0f);
            materialOpacityOverrides_[currentMaterial] = opacity;
            if (opacity < 0.995f) {
                std::cerr << "[Model] MTL opacity: " << currentMaterial << " Tr=" << value
                          << " opacity=" << opacity << '\n';
            }
        }
    }
}

void Model::processNode(aiNode* node, const aiScene* scene, const glm::mat4& parentTransform)
{
    const glm::mat4 nodeTransform = parentTransform * toGlmMat4(node->mTransformation);

    for (unsigned int i = 0; i < node->mNumMeshes; ++i) {
        aiMesh* mesh = scene->mMeshes[node->mMeshes[i]];
        meshes_.push_back(processMesh(mesh, scene, nodeTransform));

        ++meshDone_;
        if (progressCb_) {
            const float unit = static_cast<float>(meshDone_) / static_cast<float>(meshTotal_);
            const float progress = std::min(0.84f, 0.36f + 0.48f * unit);
            if (meshDone_ == 1u || (meshDone_ % 2u) == 0u || meshDone_ == meshTotal_) {
                emitProgress(progress, "Uploading mesh data to GPU...");
            }
        }
    }

    for (unsigned int i = 0; i < node->mNumChildren; ++i) {
        processNode(node->mChildren[i], scene, nodeTransform);
    }
}

Mesh Model::processMesh(aiMesh* mesh, const aiScene* scene, const glm::mat4& nodeTransform)
{
    std::vector<Vertex> vertices;
    std::vector<unsigned int> indices;
    std::vector<TextureAsset> textures;
    Material materialData;

    vertices.reserve(mesh->mNumVertices);
    indices.reserve(mesh->mNumFaces * 3);
    glm::mat3 normalMatrix(1.0f);
    const glm::mat3 linearTransform(nodeTransform);
    const float det = glm::determinant(linearTransform);
    if (std::abs(det) > 1e-8f) {
        normalMatrix = glm::transpose(glm::inverse(linearTransform));
    }
    for (unsigned int i = 0; i < mesh->mNumVertices; ++i) {
        Vertex vertex;
        const glm::vec3 localPosition(mesh->mVertices[i].x, mesh->mVertices[i].y, mesh->mVertices[i].z);
        vertex.position = glm::vec3(nodeTransform * glm::vec4(localPosition, 1.0f));

        if (mesh->HasNormals()) {
            const glm::vec3 localNormal(mesh->mNormals[i].x, mesh->mNormals[i].y, mesh->mNormals[i].z);
            vertex.normal = normalizeOrFallback(normalMatrix * localNormal, glm::vec3(0.0f, 1.0f, 0.0f));
        } else {
            vertex.normal = glm::vec3(0.0f, 1.0f, 0.0f);
        }

        if (mesh->HasTangentsAndBitangents()) {
            const glm::vec3 localTangent(mesh->mTangents[i].x, mesh->mTangents[i].y, mesh->mTangents[i].z);
            const glm::vec3 localBitangent(mesh->mBitangents[i].x, mesh->mBitangents[i].y, mesh->mBitangents[i].z);
            vertex.tangent = normalizeOrFallback(normalMatrix * localTangent, glm::vec3(1.0f, 0.0f, 0.0f));
            vertex.bitangent = normalizeOrFallback(normalMatrix * localBitangent, glm::vec3(0.0f, 0.0f, 1.0f));
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

    if (!vertices.empty()) {
        glm::vec3 meshMin = vertices[0].position;
        glm::vec3 meshMax = vertices[0].position;
        for (std::size_t vi = 1; vi < vertices.size(); ++vi) {
            meshMin = glm::min(meshMin, vertices[vi].position);
            meshMax = glm::max(meshMax, vertices[vi].position);
        }
        if (!localAabbValid_) {
            localAabbMin_ = meshMin;
            localAabbMax_ = meshMax;
            localAabbValid_ = true;
        } else {
            localAabbMin_ = glm::min(localAabbMin_, meshMin);
            localAabbMax_ = glm::max(localAabbMax_, meshMax);
        }
    }

    if (mesh->mMaterialIndex < scene->mNumMaterials && scene->mMaterials[mesh->mMaterialIndex] != nullptr) {
        aiMaterial* material = scene->mMaterials[mesh->mMaterialIndex];
        std::string materialName;
        aiString assimpMaterialName;
        if (material->Get(AI_MATKEY_NAME, assimpMaterialName) == AI_SUCCESS) {
            materialName = assimpMaterialName.C_Str();
        }

        aiColor3D diffuseColor(0.8f, 0.8f, 0.8f);
        if (material->Get(AI_MATKEY_COLOR_DIFFUSE, diffuseColor) == AI_SUCCESS) {
            materialData.diffuse = glm::vec3(diffuseColor.r, diffuseColor.g, diffuseColor.b);
        }
        aiColor3D specularColor(0.5f, 0.5f, 0.5f);
        if (material->Get(AI_MATKEY_COLOR_SPECULAR, specularColor) == AI_SUCCESS) {
            materialData.specular = glm::vec3(specularColor.r, specularColor.g, specularColor.b);
        }
        float shininess = 32.0f;
        if (material->Get(AI_MATKEY_SHININESS, shininess) == AI_SUCCESS) {
            materialData.shininess = std::clamp(shininess, 1.0f, 256.0f);
        }
        float opacity = 1.0f;
        if (material->Get(AI_MATKEY_OPACITY, opacity) == AI_SUCCESS) {
            materialData.opacity = std::clamp(opacity, 0.0f, 1.0f);
        }
        const auto opacityOverride = materialOpacityOverrides_.find(materialName);
        if (opacityOverride != materialOpacityOverrides_.end()) {
            materialData.opacity = opacityOverride->second;
        }

        aiColor3D emissiveColor(0.0f, 0.0f, 0.0f);
        if (material->Get(AI_MATKEY_COLOR_EMISSIVE, emissiveColor) == AI_SUCCESS) {
            materialData.emissive = glm::vec3(emissiveColor.r, emissiveColor.g, emissiveColor.b);
            if (glm::dot(materialData.emissive, materialData.emissive) > 0.0f) {
                std::cerr << "[Model] Emissive color: " << materialName
                          << " = (" << materialData.emissive.r << ", "
                          << materialData.emissive.g << ", "
                          << materialData.emissive.b << ")\n";
            }
        }
        float emissiveIntensity = 1.0f;
        if (material->Get(AI_MATKEY_EMISSIVE_INTENSITY, emissiveIntensity) == AI_SUCCESS) {
            materialData.emissive *= emissiveIntensity;
            if (emissiveIntensity != 1.0f) {
                std::cerr << "[Model] Emissive intensity: " << materialName
                          << " = " << emissiveIntensity << '\n';
            }
        }

        auto diffuseTextures = loadMaterialTextures(material, aiTextureType_DIFFUSE, "texture_diffuse", scene);
        textures.insert(textures.end(), diffuseTextures.begin(), diffuseTextures.end());
        auto specularTextures = loadMaterialTextures(material, aiTextureType_SPECULAR, "texture_specular", scene);
        textures.insert(textures.end(), specularTextures.begin(), specularTextures.end());
        auto shininessTextures = loadMaterialTextures(material, aiTextureType_SHININESS, "texture_roughness", scene);
        textures.insert(textures.end(), shininessTextures.begin(), shininessTextures.end());
        auto emissiveTextures = loadMaterialTextures(material, aiTextureType_EMISSIVE, "texture_emissive", scene);
        textures.insert(textures.end(), emissiveTextures.begin(), emissiveTextures.end());
        auto normalTextures = loadMaterialTextures(material, aiTextureType_NORMALS, "texture_normal", scene);
        textures.insert(textures.end(), normalTextures.begin(), normalTextures.end());
        auto heightTextures = loadMaterialTextures(material, aiTextureType_HEIGHT, "texture_normal", scene);
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

        const std::string assimpPath = aiPath.C_Str();
        std::string rawPath = extractTexturePathToken(assimpPath);

        // GLB/glTF 优先路径：Assimp 返回的纹理路径可能是 "*0"、索�?URI 或内部资源名�?
        // 这里必须先用原始 aiString 查询内嵌纹理，不能提前按 OBJ/MTL 规则裁剪路径�?
        const aiTexture* embeddedTexture = scene ? scene->GetEmbeddedTexture(assimpPath.c_str()) : nullptr;
        if (embeddedTexture == nullptr && rawPath != assimpPath) {
            embeddedTexture = scene ? scene->GetEmbeddedTexture(rawPath.c_str()) : nullptr;
        }
        if (embeddedTexture != nullptr) {
            // 内嵌纹理�?aiTexture* 去重，避免同一 GLB buffer 被多个材质槽重复解码/上传�?
            const std::string cacheKey = makeEmbeddedTextureKey(embeddedTexture);
            auto cached = textureCache_.find(cacheKey);
            if (cached != textureCache_.end()) {
                textures.push_back(TextureAsset{cached->second, typeName, assimpPath});
                continue;
            }

            const GLuint glId = uploadEmbeddedTexture(embeddedTexture, assimpPath, flipTexturesVertically_);
            if (glId != 0) {
                textureCache_[cacheKey] = glId;
                TextureAsset texture{glId, typeName, assimpPath};
                textures.push_back(texture);
                loadedTextures_.push_back(texture);
            } else {
                std::cerr << "[Model] Embedded texture decode failed: " << assimpPath << "\n";
            }
            continue;
        }

        const std::string indexedPath = !assimpPath.empty() && assimpPath[0] == '*' ? assimpPath : rawPath;
        if (indexedPath.size() >= 2 && indexedPath[0] == '*' && indexedPath[1] >= '0' && indexedPath[1] <= '9' && scene) {
            const int idx = std::stoi(indexedPath.substr(1));
            if (idx >= 0 && idx < static_cast<int>(scene->mNumTextures) && scene->mTextures[idx] != nullptr) {
                const aiTexture* tex = scene->mTextures[idx];
                const std::string cacheKey = makeEmbeddedTextureKey(tex);
                auto cached = textureCache_.find(cacheKey);
                if (cached != textureCache_.end()) {
                    textures.push_back(TextureAsset{cached->second, typeName, indexedPath});
                    continue;
                }

                const GLuint glId = uploadEmbeddedTexture(tex, indexedPath, flipTexturesVertically_);
                if (glId != 0) {
                    textureCache_[cacheKey] = glId;
                    TextureAsset texture{glId, typeName, indexedPath};
                    textures.push_back(texture);
                    loadedTextures_.push_back(texture);
                } else {
                    std::cerr << "[Model] Embedded texture decode failed: " << indexedPath << "\n";
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
        texture.id = loadTexture2DFromFile(normalized, flipTexturesVertically_);
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

void Model::appendWorldMeshAABBs(const glm::mat4& modelMatrix, const std::string& namePrefix,
                                std::vector<NamedAABB>& out) const
{
    for (std::size_t i = 0; i < meshes_.size(); ++i) {
        const Mesh& mesh = meshes_[i];
        if (!mesh.hasLocalBounds()) {
            continue;
        }

        const AABB world = AABB::fromLocalWithTransform(
            mesh.localBoundsMin(),
            mesh.localBoundsMax(),
            modelMatrix);
        out.push_back({namePrefix + "/mesh_" + std::to_string(i), world});
    }
}

void Model::appendEmissiveMeshCenters(const glm::mat4& modelMatrix, std::vector<EmissiveMeshInfo>& out) const
{
    for (const Mesh& mesh : meshes_) {
        if (!mesh.hasLocalBounds() || !mesh.isEmissive()) {
            continue;
        }

        const glm::vec3 localCenter = 0.5f * (mesh.localBoundsMin() + mesh.localBoundsMax());
        const glm::vec3 worldCenter = glm::vec3(modelMatrix * glm::vec4(localCenter, 1.0f));
        out.push_back({worldCenter, mesh.materialEmissive()});
    }
}

void Model::worldBounds(const glm::mat4& modelMatrix, glm::vec3& outMin, glm::vec3& outMax) const
{
    outMin = glm::vec3(std::numeric_limits<float>::max());
    outMax = glm::vec3(std::numeric_limits<float>::lowest());
    for (const Mesh& mesh : meshes_) {
        mesh.accumulateWorldBounds(modelMatrix, outMin, outMax);
    }
}
