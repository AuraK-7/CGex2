#include "Mesh.h"

#include <GLFW/glfw3.h>

#include <cstddef>
#include <string>
#include <utility>

Mesh::Mesh(std::vector<Vertex> vertices, std::vector<unsigned int> indices, std::vector<TextureAsset> textures, Material material)
    : vertices_(std::move(vertices))
    , indices_(std::move(indices))
    , textures_(std::move(textures))
    , material_(material)
    , indexCount_(static_cast<GLsizei>(indices_.size()))
{
    computeLocalBounds();
    setupMesh();
    releaseCpuMeshData();
}

Mesh::~Mesh()
{
    releaseGlObjects();
}

Mesh::Mesh(Mesh&& other) noexcept
    : vertices_(std::move(other.vertices_))
    , indices_(std::move(other.indices_))
    , textures_(std::move(other.textures_))
    , material_(other.material_)
    , vao_(std::exchange(other.vao_, 0))
    , vbo_(std::exchange(other.vbo_, 0))
    , ebo_(std::exchange(other.ebo_, 0))
    , indexCount_(std::exchange(other.indexCount_, 0))
    , localBoundsValid_(other.localBoundsValid_)
    , localBoundsMin_(other.localBoundsMin_)
    , localBoundsMax_(other.localBoundsMax_)
{
}

Mesh& Mesh::operator=(Mesh&& other) noexcept
{
    if (this != &other) {
        releaseGlObjects();

        vertices_ = std::move(other.vertices_);
        indices_ = std::move(other.indices_);
        textures_ = std::move(other.textures_);
        material_ = other.material_;

        vao_ = std::exchange(other.vao_, 0);
        vbo_ = std::exchange(other.vbo_, 0);
        ebo_ = std::exchange(other.ebo_, 0);
        indexCount_ = std::exchange(other.indexCount_, 0);
        localBoundsValid_ = other.localBoundsValid_;
        localBoundsMin_ = other.localBoundsMin_;
        localBoundsMax_ = other.localBoundsMax_;
    }

    return *this;
}

void Mesh::draw(const Shader& shader) const
{
    bool hasDiffuseTexture = false;
    bool hasNormalTexture = false;
    bool hasEmissiveTexture = false;

    for (const TextureAsset& texture : textures_) {
        if (texture.id == 0) {
            continue;
        }
        if (texture.type == "texture_diffuse" && !hasDiffuseTexture) {
            glActiveTexture(GL_TEXTURE0);
            glBindTexture(GL_TEXTURE_2D, texture.id);
            shader.setInt("texture_diffuse1", 0);
            hasDiffuseTexture = true;
        } else if (texture.type == "texture_normal" && !hasNormalTexture) {
            glActiveTexture(GL_TEXTURE1);
            glBindTexture(GL_TEXTURE_2D, texture.id);
            shader.setInt("texture_normal1", 1);
            hasNormalTexture = true;
        } else if (texture.type == "texture_emissive" && !hasEmissiveTexture) {
            glActiveTexture(GL_TEXTURE2);
            glBindTexture(GL_TEXTURE_2D, texture.id);
            shader.setInt("texture_emissive1", 2);
            hasEmissiveTexture = true;
        }
    }

    shader.setBool("uHasTexture", hasDiffuseTexture);
    shader.setBool("uHasNormalMap", hasNormalTexture);
    shader.setBool("uHasEmissiveMap", hasEmissiveTexture);
    shader.setVec3("uMaterialDiffuse", material_.diffuse);
    shader.setVec3("uMaterialSpecular", material_.specular);
    shader.setFloat("uMaterialShininess", material_.shininess);
    shader.setFloat("uMaterialAlpha", material_.opacity);
    shader.setVec3("uMaterialEmissive", material_.emissive);

    const bool transparent = isTransparent();
    const GLboolean blendWasEnabled = glIsEnabled(GL_BLEND);
    const GLboolean cullFaceWasEnabled = glIsEnabled(GL_CULL_FACE);
    GLboolean previousDepthMask = GL_TRUE;
    if (transparent) {
        glGetBooleanv(GL_DEPTH_WRITEMASK, &previousDepthMask);
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        glDisable(GL_CULL_FACE);
        glDepthMask(GL_FALSE);
    }

    drawDirect();

    if (transparent) {
        glDepthMask(previousDepthMask);
        if (blendWasEnabled) {
            glEnable(GL_BLEND);
        } else {
            glDisable(GL_BLEND);
        }
        if (cullFaceWasEnabled) {
            glEnable(GL_CULL_FACE);
        } else {
            glDisable(GL_CULL_FACE);
        }
    }

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, 0);
    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, 0);
    glActiveTexture(GL_TEXTURE2);
    glBindTexture(GL_TEXTURE_2D, 0);
    glActiveTexture(GL_TEXTURE0);
}

void Mesh::drawDirect() const
{
    if (vao_ == 0) {
        return;
    }

    glBindVertexArray(vao_);
    glDrawElements(GL_TRIANGLES, indexCount_, GL_UNSIGNED_INT, nullptr);
    glBindVertexArray(0);
}

void Mesh::computeLocalBounds()
{
    if (vertices_.empty()) {
        localBoundsValid_ = false;
        localBoundsMin_ = glm::vec3(0.0f);
        localBoundsMax_ = glm::vec3(0.0f);
        return;
    }

    localBoundsMin_ = vertices_[0].position;
    localBoundsMax_ = vertices_[0].position;
    for (const Vertex& vertex : vertices_) {
        localBoundsMin_ = glm::min(localBoundsMin_, vertex.position);
        localBoundsMax_ = glm::max(localBoundsMax_, vertex.position);
    }
    localBoundsValid_ = true;
}

void Mesh::setupMesh()
{
    indexCount_ = static_cast<GLsizei>(indices_.size());

    glGenBuffers(1, &vbo_);
    glGenBuffers(1, &ebo_);

    glBindBuffer(GL_ARRAY_BUFFER, vbo_);
    glBufferData(GL_ARRAY_BUFFER, static_cast<GLsizeiptr>(vertices_.size() * sizeof(Vertex)), vertices_.data(), GL_STATIC_DRAW);

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo_);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, static_cast<GLsizeiptr>(indices_.size() * sizeof(unsigned int)), indices_.data(), GL_STATIC_DRAW);

    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);

    createVertexArrayForCurrentContext();
}

void Mesh::createVertexArrayForCurrentContext()
{
    if (vao_ != 0 || vbo_ == 0 || ebo_ == 0) {
        return;
    }

    glGenVertexArrays(1, &vao_);
    glBindVertexArray(vao_);

    glBindBuffer(GL_ARRAY_BUFFER, vbo_);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo_);

    setupVertexAttributes();

    glBindVertexArray(0);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
}

void Mesh::releaseVertexArrayForCurrentContext()
{
    if (glfwGetCurrentContext() == nullptr) {
        return;
    }
    if (vao_ != 0) {
        glDeleteVertexArrays(1, &vao_);
        vao_ = 0;
    }
}

void Mesh::setupVertexAttributes() const
{
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), reinterpret_cast<void*>(offsetof(Vertex, position)));

    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), reinterpret_cast<void*>(offsetof(Vertex, normal)));

    glEnableVertexAttribArray(2);
    glVertexAttribPointer(2, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), reinterpret_cast<void*>(offsetof(Vertex, tangent)));

    glEnableVertexAttribArray(3);
    glVertexAttribPointer(3, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), reinterpret_cast<void*>(offsetof(Vertex, bitangent)));

    glEnableVertexAttribArray(4);
    glVertexAttribPointer(4, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex), reinterpret_cast<void*>(offsetof(Vertex, texCoords)));
}

void Mesh::accumulateWorldBounds(const glm::mat4& model, glm::vec3& outMin, glm::vec3& outMax) const
{
    if (!localBoundsValid_) {
        return;
    }

    const glm::vec3 mn = localBoundsMin_;
    const glm::vec3 mx = localBoundsMax_;
    const glm::vec3 corners[8] = {
        {mn.x, mn.y, mn.z},
        {mx.x, mn.y, mn.z},
        {mn.x, mx.y, mn.z},
        {mx.x, mx.y, mn.z},
        {mn.x, mn.y, mx.z},
        {mx.x, mn.y, mx.z},
        {mn.x, mx.y, mx.z},
        {mx.x, mx.y, mx.z},
    };

    for (const glm::vec3& corner : corners) {
        const glm::vec3 wp = glm::vec3(model * glm::vec4(corner, 1.0f));
        outMin = glm::min(outMin, wp);
        outMax = glm::max(outMax, wp);
    }
}

void Mesh::releaseCpuMeshData()
{
    // Vertices/indices are already uploaded to VBO/EBO; rendering only needs GPU buffers and indexCount_.
    // Large assets otherwise keep a significant duplicate CPU-side copy alive.
    std::vector<Vertex>().swap(vertices_);
    std::vector<unsigned int>().swap(indices_);
}

void Mesh::releaseGlObjects()
{
    if (glfwGetCurrentContext() == nullptr) {
        vao_ = 0;
        vbo_ = 0;
        ebo_ = 0;
        return;
    }

    if (vao_ != 0) {
        glDeleteVertexArrays(1, &vao_);
        vao_ = 0;
    }
    if (ebo_ != 0) {
        glDeleteBuffers(1, &ebo_);
        ebo_ = 0;
    }
    if (vbo_ != 0) {
        glDeleteBuffers(1, &vbo_);
        vbo_ = 0;
    }
}

void Mesh::attachTextureIfMissing(const TextureAsset& texture)
{
    for (const TextureAsset& existing : textures_) {
        if (existing.type == "texture_diffuse") {
            return;
        }
    }
    textures_.push_back(texture);
}

bool Mesh::hasTextureType(const std::string& type) const
{
    for (const TextureAsset& existing : textures_) {
        if (existing.id != 0 && existing.type == type) {
            return true;
        }
    }
    return false;
}
