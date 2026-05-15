#include "scene/Scene.h"
#include "render/Shader.h"

#include <GLFW/glfw3.h>

#include <iostream>
#include <limits>

void Scene::loadAll(const std::string& fallbackTexturePath)
{
    for (const auto& entry : entries_) {
        if (modelCache_.count(entry.path)) continue;
        std::cout << "[Scene] Loading " << entry.name << "...\n";
        auto m = std::make_unique<Model>(entry.path, fallbackTexturePath);
        glfwPollEvents();
        if (!m->isLoaded()) {
            std::cerr << "[Scene] Failed: " << entry.name << " (" << entry.path << ")\n";
            continue;
        }
        modelCache_[entry.path] = std::move(m);
    }
}

void Scene::drawAll(Shader& shader, bool geometryOnly) const
{
    for (const auto& entry : entries_) {
        auto it = modelCache_.find(entry.path);
        if (it == modelCache_.end() || !it->second->isLoaded()) continue;
        shader.setMat4("uModel", entry.transform.matrix());
        if (geometryOnly) {
            it->second->drawGeometryOnly();
        } else {
            it->second->draw(shader);
        }
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
        glm::vec3 lmn;
        glm::vec3 lmx;
        it->second->worldBounds(entry.transform.matrix(), lmn, lmx);
        mn = glm::min(mn, lmn);
        mx = glm::max(mx, lmx);
        any = true;
    }
    if (!any) {
        return false;
    }
    outMin = mn;
    outMax = mx;
    return true;
}
