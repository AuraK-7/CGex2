#pragma once

#include "collision/AABB.h"
#include "render/Shader.h"

#include <glad/glad.h>
#include <glm/glm.hpp>

#include <vector>

class DebugAabbDrawer {
public:
    explicit DebugAabbDrawer(Shader&& shader);
    ~DebugAabbDrawer();

    DebugAabbDrawer(const DebugAabbDrawer&) = delete;
    DebugAabbDrawer& operator=(const DebugAabbDrawer&) = delete;

    void draw(const glm::mat4& projection, const glm::mat4& view,
              const std::vector<NamedAABB>& boxes, const glm::vec3& color) const;

    const Shader& shader() const { return shader_; }

private:
    Shader shader_;
    GLuint vao_ = 0;
    GLuint vbo_ = 0;
};
