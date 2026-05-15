#pragma once

#include "render/Mesh.h"

#include <assimp/scene.h>
#include <glm/glm.hpp>

#include <functional>
#include <string>
#include <vector>

class Model {
public:
    using LoadProgressCallback = std::function<void(float normalized, const char* status)>;

    explicit Model(const std::string& modelPath,
                     const std::string& fallbackDiffuseTexturePath = std::string{},
                     LoadProgressCallback onProgress = {});
    ~Model();

    Model(const Model&) = delete;
    Model& operator=(const Model&) = delete;
    Model(Model&&) = delete;
    Model& operator=(Model&&) = delete;

    bool isLoaded() const noexcept { return loaded_; }
    void draw(const Shader& shader) const;

    bool hasLocalAabb() const noexcept { return localAabbValid_; }
    const glm::vec3& localAabbMin() const noexcept { return localAabbMin_; }
    const glm::vec3& localAabbMax() const noexcept { return localAabbMax_; }

private:
    std::vector<Mesh> meshes_;
    std::vector<TextureAsset> loadedTextures_;
    std::string directory_;
    bool loaded_ = false;

    bool localAabbValid_ = false;
    glm::vec3 localAabbMin_{0.0f};
    glm::vec3 localAabbMax_{0.0f};

    LoadProgressCallback progressCb_;
    unsigned meshTotal_ = 1;
    unsigned meshDone_ = 0;

    void loadModel(const std::string& modelPath);
    void emitProgress(float normalized, const char* status);
    void processNode(aiNode* node, const aiScene* scene);
    Mesh processMesh(aiMesh* mesh, const aiScene* scene);
    std::vector<TextureAsset> loadMaterialTextures(aiMaterial* material, aiTextureType type, const std::string& typeName, const aiScene* scene);
    void applyFallbackDiffuseTexture(const std::string& fallbackDiffuseTexturePath);
};
