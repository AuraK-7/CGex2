#pragma once

#include "collision/AABB.h"
#include "render/Mesh.h"

#include <assimp/scene.h>
#include <glm/glm.hpp>
#include <glm/mat4x4.hpp>
#include <glm/vec3.hpp>

#include <functional>
#include <string>
#include <unordered_map>
#include <vector>

struct TransparentMeshDraw {
    const Mesh* mesh = nullptr;
    glm::mat4 modelMatrix{1.0f};
    float distanceSquared = 0.0f;
};

struct EmissiveMeshInfo {
    glm::vec3 center{0.0f};
    glm::vec3 color{0.0f};
};

class Model {
public:
    using LoadProgressCallback = std::function<void(float normalized, const char* status)>;

    explicit Model(const std::string& modelPath, LoadProgressCallback onProgress = {});
    ~Model();

    Model(const Model&) = delete;
    Model& operator=(const Model&) = delete;
    Model(Model&&) = delete;
    Model& operator=(Model&&) = delete;

    bool isLoaded() const noexcept { return loaded_; }
    void draw(const Shader& shader) const;
    void drawOpaque(const Shader& shader) const;
    void drawGeometryOnly() const;
    void appendTransparentDraws(const glm::mat4& modelMatrix, const glm::vec3& viewPosition,
                                std::vector<TransparentMeshDraw>& out) const;

    void createVertexArraysForCurrentContext();
    void releaseVertexArraysForCurrentContext();

    bool hasLocalAabb() const noexcept { return localAabbValid_; }
    const glm::vec3& localAabbMin() const noexcept { return localAabbMin_; }
    const glm::vec3& localAabbMax() const noexcept { return localAabbMax_; }
    void appendWorldMeshAABBs(const glm::mat4& modelMatrix, const std::string& namePrefix,
                              std::vector<NamedAABB>& out) const;
    void appendEmissiveMeshCenters(const glm::mat4& modelMatrix, std::vector<EmissiveMeshInfo>& out) const;
    void worldBounds(const glm::mat4& modelMatrix, glm::vec3& outMin, glm::vec3& outMax) const;

private:
    std::vector<Mesh> meshes_;
    std::vector<TextureAsset> loadedTextures_;
    std::unordered_map<std::string, GLuint> textureCache_;
    std::unordered_map<std::string, float> materialOpacityOverrides_;
    std::string directory_;
    bool flipTexturesVertically_ = true;
    bool loaded_ = false;

    bool localAabbValid_ = false;
    glm::vec3 localAabbMin_{0.0f};
    glm::vec3 localAabbMax_{0.0f};

    LoadProgressCallback progressCb_;
    unsigned meshTotal_ = 1;
    unsigned meshDone_ = 0;

    void loadModel(const std::string& modelPath);
    void loadMaterialOpacityOverrides(const std::string& modelPath);
    void emitProgress(float normalized, const char* status);
    void processNode(aiNode* node, const aiScene* scene, const glm::mat4& parentTransform);
    Mesh processMesh(aiMesh* mesh, const aiScene* scene, const glm::mat4& nodeTransform);
    std::vector<TextureAsset> loadMaterialTextures(aiMaterial* material, aiTextureType type, const std::string& typeName, const aiScene* scene);
};
