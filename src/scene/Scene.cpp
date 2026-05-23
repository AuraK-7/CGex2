#include "scene/Scene.h"
#include "render/Shader.h"

#include <algorithm>
#include <iostream>
#include <limits>
#include <vector>

void Scene::loadAll(const std::function<void(float, const char*)>& onProgress)
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
        if (modelCache_.count(entry.path)) {
            continue;
        }

        std::cout << "[Scene] Loading " << entry.name << "...\n";
        const size_t currentIndex = modelIndex++;
        auto scaledProgress = [onProgress, currentIndex, pendingCount](float t, const char* msg) {
            if (!onProgress) {
                return;
            }
            const float span = 1.0f / static_cast<float>(pendingCount);
            const float global = (static_cast<float>(currentIndex) + std::clamp(t, 0.0f, 1.0f)) * span;
            onProgress(std::min(1.0f, global), msg);
        };

        auto model = std::make_unique<Model>(entry.path, scaledProgress);
        if (!model->isLoaded()) {
            std::cerr << "[Scene] Failed: " << entry.name << " (" << entry.path << ")\n";
            continue;
        }
        modelCache_[entry.path] = std::move(model);
    }

    if (onProgress) {
        onProgress(1.0f, "Scene load complete.");
    }
}

void Scene::drawAll(Shader& shader, bool geometryOnly, const glm::vec3* viewPosition) const
{
    std::vector<TransparentMeshDraw> transparentDraws;

    for (const auto& entry : entries_) {
        auto it = modelCache_.find(entry.path);
        if (it == modelCache_.end() || !it->second->isLoaded()) {
            continue;
        }

        const glm::mat4 modelMatrix = entry.transform.matrix();
        shader.setMat4("uModel", modelMatrix);
        if (geometryOnly) {
            it->second->drawGeometryOnly();
        } else if (viewPosition != nullptr) {
            it->second->drawOpaque(shader);
            it->second->appendTransparentDraws(modelMatrix, *viewPosition, transparentDraws);
        } else {
            it->second->draw(shader);
        }
    }

    if (geometryOnly || viewPosition == nullptr || transparentDraws.empty()) {
        return;
    }

    std::sort(transparentDraws.begin(), transparentDraws.end(),
              [](const TransparentMeshDraw& a, const TransparentMeshDraw& b) {
                  return a.distanceSquared > b.distanceSquared;
              });

    for (const TransparentMeshDraw& draw : transparentDraws) {
        if (draw.mesh == nullptr) {
            continue;
        }
        shader.setMat4("uModel", draw.modelMatrix);
        draw.mesh->draw(shader);
    }
}

void Scene::createVertexArraysForCurrentContext()
{
    for (auto& [path, model] : modelCache_) {
        (void)path;
        if (model) {
            model->createVertexArraysForCurrentContext();
        }
    }
}

void Scene::releaseVertexArraysForCurrentContext()
{
    for (auto& [path, model] : modelCache_) {
        (void)path;
        if (model) {
            model->releaseVertexArraysForCurrentContext();
        }
    }
}

std::vector<NamedAABB> Scene::namedWorldAABBs() const
{
    std::vector<NamedAABB> out;
    for (const auto& entry : entries_) {
        auto it = modelCache_.find(entry.path);
        if (it == modelCache_.end() || !it->second->isLoaded()) {
            continue;
        }
        it->second->appendWorldMeshAABBs(entry.transform.matrix(), entry.name, out);
    }
    return out;
}

std::vector<EmissiveMeshInfo> Scene::emissiveMeshCenters() const
{
    std::vector<EmissiveMeshInfo> out;
    for (const auto& entry : entries_) {
        auto it = modelCache_.find(entry.path);
        if (it == modelCache_.end() || !it->second->isLoaded()) {
            continue;
        }
        it->second->appendEmissiveMeshCenters(entry.transform.matrix(), out);
    }
    return out;
}

bool Scene::computeWorldBounds(glm::vec3& outMin, glm::vec3& outMax) const
{
    glm::vec3 mn(std::numeric_limits<float>::max());
    glm::vec3 mx(std::numeric_limits<float>::lowest());
    bool any = false;

    for (const auto& entry : entries_) {
        auto it = modelCache_.find(entry.path);
        if (it == modelCache_.end() || !it->second->isLoaded()) {
            continue;
        }

        glm::vec3 localMin;
        glm::vec3 localMax;
        it->second->worldBounds(entry.transform.matrix(), localMin, localMax);
        mn = glm::min(mn, localMin);
        mx = glm::max(mx, localMax);
        any = true;
    }

    if (!any) {
        return false;
    }

    outMin = mn;
    outMax = mx;
    return true;
}
