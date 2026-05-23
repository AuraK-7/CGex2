#pragma once

#include <glm/glm.hpp>

#include <string>
#include <vector>

struct DirLight {
    glm::vec3 direction{ -0.2f, -1.0f, -0.3f };
    glm::vec3 ambient{ 0.1f };
    glm::vec3 diffuse{ 0.5f };
    glm::vec3 specular{ 0.2f };
};

struct PointLight {
    std::string name;
    glm::vec3 position{ 0.0f };
    glm::vec3 color{ 1.0f };
    float intensity = 1.0f;
    float constant = 1.0f;
    float linear = 0.09f;
    float quadratic = 0.032f;
};

class LightManager {
public:
    glm::vec3 globalAmbient{ 0.05f };
    DirLight sunLight;
    std::vector<PointLight> pointLights;
    float directionalStrength = 1.8f;
    float pointLightStrength = 1.0f;
    float spotLightStrength = 1.0f;

    bool loadConfig(const std::string& path);
    bool saveConfig(const std::string& path) const;
    void sendToShader(unsigned int shaderID) const;
};
