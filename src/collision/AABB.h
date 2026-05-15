#pragma once

#include <glm/glm.hpp>

#include <string>
#include <vector>

struct AABB {
    glm::vec3 min{0.0f};
    glm::vec3 max{0.0f};

    static AABB fromMinMax(const glm::vec3& mn, const glm::vec3& mx) noexcept
    {
        return {glm::min(mn, mx), glm::max(mn, mx)};
    }

    static AABB fromCenterHalfExtents(const glm::vec3& center, const glm::vec3& halfExtents) noexcept
    {
        return fromMinMax(center - halfExtents, center + halfExtents);
    }

    /// World-space AABB after transforming the eight corners of a local axis-aligned box.
    static AABB fromLocalWithTransform(const glm::vec3& localMin, const glm::vec3& localMax,
                                       const glm::mat4& model) noexcept;

    bool valid() const noexcept { return min.x <= max.x && min.y <= max.y && min.z <= max.z; }

    bool intersects(const AABB& o) const noexcept;
};

struct NamedAABB {
    std::string name;
    AABB box;
};

/// A few thin walls around the origin so collision is visible without splitting the classroom mesh.
void appendDemoRoomColliders(std::vector<NamedAABB>& out);
