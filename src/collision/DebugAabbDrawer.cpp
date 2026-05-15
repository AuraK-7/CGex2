#include "collision/DebugAabbDrawer.h"

#include <array>
#include <cstddef>

namespace {

void appendBoxLines(const AABB& b, std::vector<glm::vec3>& lines)
{
    const glm::vec3 mn = b.min;
    const glm::vec3 mx = b.max;
    const std::array<glm::vec3, 8> c = {{
        {mn.x, mn.y, mn.z}, {mx.x, mn.y, mn.z}, {mx.x, mx.y, mn.z}, {mn.x, mx.y, mn.z},
        {mn.x, mn.y, mx.z}, {mx.x, mn.y, mx.z}, {mx.x, mx.y, mx.z}, {mn.x, mx.y, mx.z},
    }};
    const std::array<std::pair<int, int>, 12> e = {{
        {0, 1}, {1, 2}, {2, 3}, {3, 0},
        {4, 5}, {5, 6}, {6, 7}, {7, 4},
        {0, 4}, {1, 5}, {2, 6}, {3, 7},
    }};
    for (const auto& edge : e) {
        lines.push_back(c[static_cast<std::size_t>(edge.first)]);
        lines.push_back(c[static_cast<std::size_t>(edge.second)]);
    }
}

} // namespace

DebugAabbDrawer::DebugAabbDrawer(Shader&& shader)
    : shader_(std::move(shader))
{
    glGenVertexArrays(1, &vao_);
    glGenBuffers(1, &vbo_);
    glBindVertexArray(vao_);
    glBindBuffer(GL_ARRAY_BUFFER, vbo_);
    glBufferData(GL_ARRAY_BUFFER, 0, nullptr, GL_DYNAMIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(glm::vec3), reinterpret_cast<void*>(0));
    glBindVertexArray(0);
}

DebugAabbDrawer::~DebugAabbDrawer()
{
    if (vbo_ != 0) glDeleteBuffers(1, &vbo_);
    if (vao_ != 0) glDeleteVertexArrays(1, &vao_);
}

void DebugAabbDrawer::draw(const glm::mat4& projection, const glm::mat4& view,
                           const std::vector<NamedAABB>& boxes, const glm::vec3& color) const
{
    if (!shader_.isValid() || boxes.empty()) return;

    std::vector<glm::vec3> lines;
    for (const NamedAABB& n : boxes) {
        appendBoxLines(n.box, lines);
    }
    if (lines.empty()) return;

    const glm::mat4 mvp = projection * view;

    shader_.use();
    shader_.setMat4("uMVP", mvp);
    shader_.setVec3("uColor", color);

    glBindVertexArray(vao_);
    glBindBuffer(GL_ARRAY_BUFFER, vbo_);
    glBufferData(GL_ARRAY_BUFFER,
                 static_cast<GLsizeiptr>(lines.size() * sizeof(glm::vec3)),
                 lines.data(),
                 GL_DYNAMIC_DRAW);
    glDrawArrays(GL_LINES, 0, static_cast<GLsizei>(lines.size()));
    glBindVertexArray(0);
}
