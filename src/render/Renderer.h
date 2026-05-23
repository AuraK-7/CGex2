#pragma once

#include <glm/glm.hpp>
#include "render/Shader.h"
#include "render/ShadowMap.h"
#include "render/LightManager.h"
#include "core/Camera.h"
#include "scene/Scene.h"

class Renderer {
public:
    Renderer(Shader& litShader, Shader& depthShader, Camera& camera);

    Renderer(const Renderer&) = delete;
    Renderer& operator=(const Renderer&) = delete;
    Renderer(Renderer&&) = delete;
    Renderer& operator=(Renderer&&) = delete;

    void setLightDirection(const glm::vec3& dir) { lightDir_ = glm::normalize(dir); }
    void setDirectionalShadowsEnabled(bool enabled) { directionalShadowsEnabled_ = enabled; }
    void setShadowStrength(float strength) { shadowStrength_ = strength; }

    void toggleLight() { lightOn_ = !lightOn_; }
    bool isLightOn() const { return lightOn_; }
    void toggleDayNight();
    bool isDayMode() const { return dayMode_; }

    void setLightManager(const LightManager* lightManager) { lightManager_ = lightManager; }
    void render(const Scene& scene);

private:
    Shader& litShader_;
    Shader& depthShader_;
    Camera& camera_;
    ShadowMap shadowMap_;
    const LightManager* lightManager_ = nullptr;
    glm::vec3 lightDir_{-0.8f, -1.0f, -0.3f};
    bool directionalShadowsEnabled_ = true;
    float shadowStrength_ = 0.72f;
    static constexpr int kShadowMapUnit = 3;

    bool sceneBoundsTried_ = false;
    bool sceneBoundsValid_ = false;
    glm::vec3 sceneBmin_{0.0f};
    glm::vec3 sceneBmax_{0.0f};
    bool lightOn_ = true;
    bool dayMode_ = true;
    float ambientStrength_ = 0.25f;
    glm::vec3 ambientColor_{1.0f};
};
