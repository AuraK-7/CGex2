#include "collision/AABB.h"

#include <array>
#include <limits>

AABB AABB::fromLocalWithTransform(const glm::vec3& localMin, const glm::vec3& localMax,
                                  const glm::mat4& model) noexcept
{
    const std::array<glm::vec3, 8> corners = {{
        {localMin.x, localMin.y, localMin.z},
        {localMax.x, localMin.y, localMin.z},
        {localMin.x, localMax.y, localMin.z},
        {localMax.x, localMax.y, localMin.z},
        {localMin.x, localMin.y, localMax.z},
        {localMax.x, localMin.y, localMax.z},
        {localMin.x, localMax.y, localMax.z},
        {localMax.x, localMax.y, localMax.z},
    }};

    glm::vec3 wmin(std::numeric_limits<float>::max());
    glm::vec3 wmax(std::numeric_limits<float>::lowest());

    for (const glm::vec3& c : corners) {
        const glm::vec3 p = glm::vec3(model * glm::vec4(c, 1.0f));
        wmin = glm::min(wmin, p);
        wmax = glm::max(wmax, p);
    }

    return fromMinMax(wmin, wmax);
}

bool AABB::intersects(const AABB& o) const noexcept
{
    if (!valid() || !o.valid()) return false;
    return (max.x >= o.min.x && min.x <= o.max.x)
        && (max.y >= o.min.y && min.y <= o.max.y)
        && (max.z >= o.min.z && min.z <= o.max.z);
}

void appendDemoRoomColliders(std::vector<NamedAABB>& out)
{
    // Simple “room” around spawn; south side left open. Tune after you load real assets.
    out.push_back({"demo_west", AABB::fromMinMax({-6.0f, 0.0f, -6.0f}, {-5.0f, 3.0f, 6.0f})});
    out.push_back({"demo_east", AABB::fromMinMax({5.0f, 0.0f, -6.0f}, {6.0f, 3.0f, 6.0f})});
    out.push_back({"demo_north", AABB::fromMinMax({-6.0f, 0.0f, -6.0f}, {6.0f, 3.0f, -5.0f})});
    out.push_back({"demo_ceiling", AABB::fromMinMax({-6.0f, 3.0f, -6.0f}, {6.0f, 4.0f, 6.0f})});
}
