#include "render/LightManager.h"

#include <glad/glad.h>
#include <nlohmann/json.hpp>

#include <algorithm>
#include <fstream>
#include <iostream>
#include <string>
#include <stdexcept>

using json = nlohmann::json;

namespace {

constexpr int kMaxPointLights = 10;

glm::vec3 readVec3(const json& value, const char* fieldName)
{
    if (!value.is_array() || value.size() != 3) {
        throw std::runtime_error(std::string("expected vec3 array for ") + fieldName);
    }

    return glm::vec3(
        value.at(0).get<float>(),
        value.at(1).get<float>(),
        value.at(2).get<float>());
}

json writeVec3(const glm::vec3& value)
{
    return json::array({ value.x, value.y, value.z });
}

void setVec3(unsigned int shaderID, const std::string& name, const glm::vec3& value)
{
    glUniform3fv(glGetUniformLocation(shaderID, name.c_str()), 1, &value[0]);
}

void setFloat(unsigned int shaderID, const std::string& name, float value)
{
    glUniform1f(glGetUniformLocation(shaderID, name.c_str()), value);
}

} // namespace

bool LightManager::loadConfig(const std::string& path)
{
    std::ifstream file(path);
    if (!file.is_open()) {
        std::cerr << "[Light] Could not open lighting config: " << path << '\n';
        return false;
    }

    try {
        json data;
        file >> data;

        const auto& env = data.at("environment");
        globalAmbient = readVec3(env.at("global_ambient"), "environment.global_ambient");
        pointLightStrength = env.value("point_light_strength", 1.0f);
        spotLightStrength = env.value("spot_light_strength", 1.0f);

        const auto& dir = data.at("directional_light");
        sunLight.direction = readVec3(dir.at("direction"), "directional_light.direction");
        sunLight.ambient = readVec3(dir.at("ambient"), "directional_light.ambient");
        sunLight.diffuse = readVec3(dir.at("diffuse"), "directional_light.diffuse");
        sunLight.specular = readVec3(dir.at("specular"), "directional_light.specular");
        directionalStrength = dir.value("strength", 1.8f);

        pointLights.clear();
        const auto& lights = data.at("point_lights");
        if (!lights.is_array()) {
            throw std::runtime_error("expected array for point_lights");
        }

        for (const auto& pl : lights) {
            if (pointLights.size() >= kMaxPointLights) {
                std::cerr << "[Light] Ignoring extra point light; shader supports "
                          << kMaxPointLights << " point lights.\n";
                break;
            }

            PointLight light;
            light.name = pl.value("comment", pl.value("name", std::string{}));
            light.position = readVec3(pl.at("position"), "point_lights[].position");
            const glm::vec3 rawColor = readVec3(pl.at("color"), "point_lights[].color");
            light.intensity = pl.value("intensity", 1.0f);
            light.color = rawColor * light.intensity;
            light.constant = pl.value("constant", 1.0f);
            light.linear = pl.value("linear", 0.09f);
            light.quadratic = pl.value("quadratic", 0.032f);
            pointLights.push_back(light);
        }
    } catch (const std::exception& ex) {
        std::cerr << "[Light] Failed to parse lighting config: " << path << '\n'
                  << "  Reason: " << ex.what() << '\n';
        return false;
    }

    std::cout << "[Light] Loaded lighting config with " << pointLights.size()
              << " point light(s).\n";
    return true;
}

bool LightManager::saveConfig(const std::string& path) const
{
    json data;
    data["environment"] = {
        { "name", "Scene" },
        { "global_ambient", writeVec3(globalAmbient) },
        { "point_light_strength", pointLightStrength },
        { "spot_light_strength", spotLightStrength },
    };
    data["directional_light"] = {
        { "direction", writeVec3(sunLight.direction) },
        { "ambient", writeVec3(sunLight.ambient) },
        { "diffuse", writeVec3(sunLight.diffuse) },
        { "specular", writeVec3(sunLight.specular) },
        { "strength", directionalStrength },
    };

    data["point_lights"] = json::array();
    for (const PointLight& light : pointLights) {
        const float safeIntensity = std::max(light.intensity, 0.0001f);
        const glm::vec3 rawColor = light.color / safeIntensity;
        json item = {
            { "position", writeVec3(light.position) },
            { "color", writeVec3(rawColor) },
            { "intensity", light.intensity },
            { "constant", light.constant },
            { "linear", light.linear },
            { "quadratic", light.quadratic },
        };
        if (!light.name.empty()) {
            item["comment"] = light.name;
        }
        data["point_lights"].push_back(item);
    }

    std::ofstream file(path);
    if (!file.is_open()) {
        std::cerr << "[Light] Could not save lighting config: " << path << '\n';
        return false;
    }

    file << data.dump(4) << '\n';
    std::cout << "[Light] Saved lighting config: " << path << '\n';
    return true;
}

void LightManager::sendToShader(unsigned int shaderID) const
{
    if (shaderID == 0) {
        return;
    }

    glUseProgram(shaderID);

    setVec3(shaderID, "globalAmbient", globalAmbient);
    setVec3(shaderID, "dirLight.direction", sunLight.direction);
    setVec3(shaderID, "dirLight.ambient", sunLight.ambient * directionalStrength);
    setVec3(shaderID, "dirLight.diffuse", sunLight.diffuse * directionalStrength);
    setVec3(shaderID, "dirLight.specular", sunLight.specular * directionalStrength);

    const int numLights = std::min(static_cast<int>(pointLights.size()), kMaxPointLights);
    glUniform1i(glGetUniformLocation(shaderID, "numPointLights"), numLights);

    for (int i = 0; i < numLights; ++i) {
        const std::string base = "pointLights[" + std::to_string(i) + "].";
        setVec3(shaderID, base + "position", pointLights[i].position);
        setVec3(shaderID, base + "color", pointLights[i].color * pointLightStrength);
        setFloat(shaderID, base + "constant", pointLights[i].constant);
        setFloat(shaderID, base + "linear", pointLights[i].linear);
        setFloat(shaderID, base + "quadratic", pointLights[i].quadratic);
    }
}
