#include "Mesh.h"

#include <cstddef>
#include <string>

Mesh::Mesh(std::vector<Vertex> vertices, std::vector<unsigned int> indices, std::vector<TextureAsset> textures, Material material)
    : vertices_(std::move(vertices))
    , indices_(std::move(indices))
    , textures_(std::move(textures))
    , material_(material)
{
    setupMesh();
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
    }

    return *this;
}

void Mesh::draw(const Shader& shader) const
{
    bool hasDiffuseTexture = false;
    bool hasNormalTexture = false;
    unsigned int diffuseIndex = 1;
    unsigned int normalIndex = 1;

    for (unsigned int i = 0; i < textures_.size(); ++i) {
        if (textures_[i].id == 0) continue;
        glActiveTexture(GL_TEXTURE0 + i);
        glBindTexture(GL_TEXTURE_2D, textures_[i].id);
        if (textures_[i].type == "texture_diffuse") {
            shader.setInt("texture_diffuse" + std::to_string(diffuseIndex), static_cast<int>(i));
            ++diffuseIndex;
            hasDiffuseTexture = true;
        } else if (textures_[i].type == "texture_normal") {
            shader.setInt("texture_normal" + std::to_string(normalIndex), static_cast<int>(i));
            ++normalIndex;
            hasNormalTexture = true;
        }
    }
    shader.setBool("uHasTexture", hasDiffuseTexture);
    shader.setBool("uHasNormalMap", hasNormalTexture);
    shader.setVec3("uMaterialDiffuse", material_.diffuse);
    drawDirect();
    glActiveTexture(GL_TEXTURE0);
}

void Mesh::drawDirect() const
{
    if (vao_ == 0) {
        return;
    }

    glBindVertexArray(vao_);
    glDrawElements(GL_TRIANGLES, static_cast<GLsizei>(indices_.size()), GL_UNSIGNED_INT, nullptr);
    glBindVertexArray(0);
}

void Mesh::setupMesh()
{
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
    for (const Vertex& v : vertices_) {
        const glm::vec3 wp = glm::vec3(model * glm::vec4(v.position, 1.0f));
        outMin = glm::min(outMin, wp);
        outMax = glm::max(outMax, wp);
    }
}

void Mesh::releaseGlObjects()
{
    if (ebo_ != 0) {
        glDeleteBuffers(1, &ebo_);
        ebo_ = 0;
    }
    if (vbo_ != 0) {
        glDeleteBuffers(1, &vbo_);
        vbo_ = 0;
    }
    if (vao_ != 0) {
        glDeleteVertexArrays(1, &vao_);
        vao_ = 0;
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
