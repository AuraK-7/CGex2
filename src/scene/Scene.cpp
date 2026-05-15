#include "scene/Scene.h"
#include "collision/AABB.h"
#include "render/Shader.h"
#include <GLFW/glfw3.h>
#include <algorithm>
#include <iostream>

void Scene::loadAll(const std::string& fallbackTex,
                    const std::function<void(float, const char*)>& onProgress)
{
    size_t pendingCount = 0;
    for (const auto& entry : entries_) {
        if (!modelCache_.count(entry.path)) {
            ++pendingCount;
        }
    }
    if (pendingCount == 0) {
        if (onProgress) {
            onProgress(1.0f, "All models already in memory.");
        }
        return;
    }

    size_t modelIndex = 0;
    for (const auto& entry : entries_) {
        if (modelCache_.count(entry.path)) continue;
        std::cout << "[Scene] Loading " << entry.name << "...\n";
        const size_t k = modelIndex++;
        auto scaledProgress = [onProgress, k, pendingCount](float t, const char* msg) {
            if (!onProgress) {
                return;
            }
            const float span = 1.0f / static_cast<float>(pendingCount);
            const float g = (static_cast<float>(k) + std::clamp(t, 0.0f, 1.0f)) * span;
            onProgress(std::min(1.0f, g), msg);
        };
        auto m = std::make_unique<Model>(entry.path, fallbackTex, scaledProgress);
        glfwPollEvents();
        if (!m->isLoaded()) {
            std::cerr << "[Scene] Failed: " << entry.name << " (" << entry.path << ")\n";
            continue;
        }
        modelCache_[entry.path] = std::move(m);
    }
    if (onProgress) {
        onProgress(1.0f, "Scene load complete.");
    }
}

void Scene::drawAll(Shader& shader) const
{
    for (const auto& entry : entries_) {
        auto it = modelCache_.find(entry.path);
        if (it == modelCache_.end() || !it->second->isLoaded()) continue;
        shader.setMat4("uModel", entry.transform.matrix());
        it->second->draw(shader);
    }
}

std::vector<NamedAABB> Scene::namedWorldAABBs() const
{
    std::vector<NamedAABB> out;
    for (const auto& entry : entries_) {
        auto it = modelCache_.find(entry.path);
        if (it == modelCache_.end() || !it->second->isLoaded() || !it->second->hasLocalAabb()) {
            continue;
        }
        const glm::mat4 m = entry.transform.matrix();
        const AABB world = AABB::fromLocalWithTransform(
            it->second->localAabbMin(), it->second->localAabbMax(), m);
        out.push_back({entry.name, world});
    }
    return out;
}
