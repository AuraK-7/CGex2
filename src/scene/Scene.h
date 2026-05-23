#pragma once

#include "collision/AABB.h"
#include "core/Transform.h"
#include "Model.h"

#include <functional>
#include <glm/vec3.hpp>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

struct ModelEntry {
    std::string path;
    Transform transform;
    std::string name;
};

class Scene {
public:
    void addModel(const ModelEntry& entry) { entries_.push_back(entry); }
    void loadAll(const std::function<void(float normalized, const char* status)>& onProgress = {});
    void drawAll(class Shader& shader, bool geometryOnly = false, const glm::vec3* viewPosition = nullptr) const;

    void createVertexArraysForCurrentContext();
    void releaseVertexArraysForCurrentContext();

    std::vector<NamedAABB> namedWorldAABBs() const;
    std::vector<EmissiveMeshInfo> emissiveMeshCenters() const;
    bool computeWorldBounds(glm::vec3& outMin, glm::vec3& outMax) const;

    const std::vector<ModelEntry>& entries() const { return entries_; }
    size_t modelCount() const { return modelCache_.size(); }

private:
    std::vector<ModelEntry> entries_;
    std::unordered_map<std::string, std::unique_ptr<Model>> modelCache_;
};
