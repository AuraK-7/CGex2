#pragma once

#include <glm/glm.hpp>
#include "render/Shader.h"
#include "render/ShadowMap.h"
#include "core/Camera.h"
#include "scene/Scene.h"

class Renderer {
public:
    Renderer(Shader& litShader, Shader& depthShader, Camera& camera);

    void setLightDirection(const glm::vec3& dir) { lightDir_ = glm::normalize(dir); }
    void setDirectionalShadowsEnabled(bool enabled) { directionalShadowsEnabled_ = enabled; }

    void render(const Scene& scene);

private:
    Shader& litShader_;
    Shader& depthShader_;
    Camera& camera_;
    ShadowMap shadowMap_;
    glm::vec3 lightDir_{-0.8f, -1.0f, -0.3f};
    bool directionalShadowsEnabled_ = true;
    static constexpr int kShadowMapUnit = 3;

    bool sceneBoundsTried_ = false;
    bool sceneBoundsValid_ = false;
    glm::vec3 sceneBmin_{0.0f};
    glm::vec3 sceneBmax_{0.0f};
};
