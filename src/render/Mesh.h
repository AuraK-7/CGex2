#pragma once

#include "Material.h"
#include "Shader.h"

#include <glad/glad.h>
#include <glm/glm.hpp>

#include <string>
#include <type_traits>
#include <vector>

struct Vertex {
    glm::vec3 position {};
    glm::vec3 normal {};
    glm::vec3 tangent {};
    glm::vec3 bitangent {};
    glm::vec2 texCoords {};
};

static_assert(std::is_standard_layout_v<Vertex>, "Vertex must stay standard-layout for offsetof.");

struct TextureAsset {
    GLuint id = 0;
    std::string type;
    std::string path;
};

class Mesh {
public:
    Mesh(std::vector<Vertex> vertices, std::vector<unsigned int> indices, std::vector<TextureAsset> textures, Material material = {});
    ~Mesh();

    Mesh(const Mesh&) = delete;
    Mesh& operator=(const Mesh&) = delete;

    Mesh(Mesh&& other) noexcept;
    Mesh& operator=(Mesh&& other) noexcept;

    void draw(const Shader& shader) const;
    void drawDirect() const;
    void attachTextureIfMissing(const TextureAsset& texture);
    bool hasTextureType(const std::string& type) const;
    bool isTransparent() const noexcept { return material_.opacity < 0.995f; }
    void createVertexArrayForCurrentContext();
    void releaseVertexArrayForCurrentContext();

    const std::vector<TextureAsset>& textures() const { return textures_; }
    glm::vec3 materialKd() const { return material_.diffuse; }
    glm::vec3 materialEmissive() const { return material_.emissive; }
    bool isEmissive() const noexcept { return glm::dot(material_.emissive, material_.emissive) > 0.0001f; }
    bool hasLocalBounds() const noexcept { return localBoundsValid_; }
    const glm::vec3& localBoundsMin() const noexcept { return localBoundsMin_; }
    const glm::vec3& localBoundsMax() const noexcept { return localBoundsMax_; }

    void accumulateWorldBounds(const glm::mat4& model, glm::vec3& outMin, glm::vec3& outMax) const;

private:
    std::vector<Vertex> vertices_;
    std::vector<unsigned int> indices_;
    std::vector<TextureAsset> textures_;
    Material material_;

    GLuint vao_ = 0;
    GLuint vbo_ = 0;
    GLuint ebo_ = 0;
    GLsizei indexCount_ = 0;
    bool localBoundsValid_ = false;
    glm::vec3 localBoundsMin_{0.0f};
    glm::vec3 localBoundsMax_{0.0f};

    void computeLocalBounds();
    void setupMesh();
    void setupVertexAttributes() const;
    void releaseCpuMeshData();
    void releaseGlObjects();
};
