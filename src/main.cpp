#include <glad/glad.h>
#include <GLFW/glfw3.h>

#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_opengl3.h>

#include "collision/AABB.h"
#include "collision/DebugAabbDrawer.h"
#include "core/Application.h"
#include "core/Camera.h"
#include "render/LightManager.h"
#include "render/Renderer.h"
#include "render/Shader.h"
#include "scene/Scene.h"

#include <algorithm>
#include <filesystem>
#include <iostream>
#include <cmath>
#include <limits>
#include <string>
#include <vector>

#include <glm/geometric.hpp>
#include <glm/gtc/matrix_transform.hpp>

namespace {

struct LightingState {
    bool isDay = true;
    bool pointLightsOn = true;
    bool key1WasDown = false;
    bool key2WasDown = false;
    bool keyOWasDown = false;
    bool keyCWasDown = false;
};

void restoreGlStateForScene(GLFWwindow* window)
{
    int w = 0;
    int h = 0;
    glfwGetFramebufferSize(window, &w, &h);
    if (w > 0 && h > 0) {
        glViewport(0, 0, w, h);
    }

    glDisable(GL_BLEND);
    glDisable(GL_SCISSOR_TEST);
    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LESS);
    glDepthMask(GL_TRUE);
    glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
    for (int i = 0; i < 8; ++i) {
        glActiveTexture(static_cast<GLenum>(GL_TEXTURE0 + i));
        glBindTexture(GL_TEXTURE_2D, 0);
    }
    glActiveTexture(GL_TEXTURE0);
    glBindVertexArray(0);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
    glUseProgram(0);
}

std::string resolvePath(const std::vector<std::string>& candidates)
{
    const std::filesystem::path current = std::filesystem::current_path();
    const std::vector<std::filesystem::path> roots{current, current / "..", current / "../.."};

    for (const auto& candidate : candidates) {
        std::filesystem::path path(candidate);
        if (path.is_absolute() && std::filesystem::exists(path)) {
            return std::filesystem::weakly_canonical(path).string();
        }
    }

    for (const auto& root : roots) {
        for (const auto& candidate : candidates) {
            const auto full = root / candidate;
            if (std::filesystem::exists(full)) {
                return std::filesystem::weakly_canonical(full).string();
            }
        }
    }
    return {};
}

AABB cameraBodyAabb(const glm::vec3& position)
{
    const glm::vec3 bodyCenter = position - glm::vec3(0.0f, 0.85f, 0.0f);
    return AABB::fromCenterHalfExtents(bodyCenter, glm::vec3(0.28f, 0.92f, 0.28f));
}

bool intersectsAny(const AABB& box, const std::vector<NamedAABB>& colliders)
{
    for (const NamedAABB& collider : colliders) {
        if (box.intersects(collider.box)) {
            return true;
        }
    }
    return false;
}

void resolveCameraCollision(Camera& camera, const glm::vec3& previousPosition,
                            const std::vector<NamedAABB>& colliders)
{
    if (colliders.empty()) {
        return;
    }

    const glm::vec3 desired = camera.Position;
    glm::vec3 candidate = previousPosition;

    auto tryAxis = [&](int axis) {
        glm::vec3 next = candidate;
        next[axis] = desired[axis];
        if (!intersectsAny(cameraBodyAabb(next), colliders)) {
            candidate = next;
        }
    };

    // Axis-separated sliding: a blocked axis rolls back while other axes can still move.
    tryAxis(0);
    tryAxis(2);
    tryAxis(1);

    camera.Position = candidate;
}

void handleLightingHotkeys(GLFWwindow* window, LightingState& lighting)
{
    const bool key1 = glfwGetKey(window, GLFW_KEY_1) == GLFW_PRESS;
    const bool key2 = glfwGetKey(window, GLFW_KEY_2) == GLFW_PRESS;
    const bool keyO = glfwGetKey(window, GLFW_KEY_O) == GLFW_PRESS;
    const bool keyC = glfwGetKey(window, GLFW_KEY_C) == GLFW_PRESS;

    if (key1 && !lighting.key1WasDown) {
        lighting.isDay = true;
    }
    if (key2 && !lighting.key2WasDown) {
        lighting.isDay = false;
    }

    if (keyO && !lighting.keyOWasDown) {
        lighting.pointLightsOn = true;
    }
    if (keyC && !lighting.keyCWasDown) {
        lighting.pointLightsOn = false;
    }

    lighting.key1WasDown = key1;
    lighting.key2WasDown = key2;
    lighting.keyOWasDown = keyO;
    lighting.keyCWasDown = keyC;
}

void syncLightingUniforms(Shader& shader, Renderer& renderer, const Camera& camera,
                          const LightingState& lighting, const LightManager& lightManager)
{
    shader.use();
    if (lighting.isDay) {
        renderer.setDirectionalShadowsEnabled(true);
        shader.setFloat("uExposure", 0.88f);
        shader.setVec3("dirLight.ambient", glm::vec3(0.10f, 0.105f, 0.12f));
        shader.setVec3("dirLight.diffuse", glm::vec3(0.88f, 0.82f, 0.68f));
        shader.setVec3("dirLight.specular", glm::vec3(0.22f, 0.20f, 0.18f));
        shader.setInt("uPointLightsOn", lighting.pointLightsOn ? 1 : 0);
    } else {
        renderer.setDirectionalShadowsEnabled(false);
        shader.setFloat("uExposure", 0.94f);
        shader.setVec3("dirLight.ambient", glm::vec3(0.02f, 0.02f, 0.05f));
        shader.setVec3("dirLight.diffuse", glm::vec3(0.05f, 0.05f, 0.10f));
        shader.setVec3("dirLight.specular", glm::vec3(0.10f, 0.10f, 0.10f));
        shader.setInt("uPointLightsOn", lighting.pointLightsOn ? 1 : 0);
    }

    const float lampEmissionScale = lighting.pointLightsOn ? lightManager.pointLightStrength : 0.0f;
    shader.setFloat("uLampEmissionScale", lampEmissionScale);

    shader.setVec3("spotLight.position", camera.Position);
    shader.setVec3("spotLight.direction", camera.Front);
    shader.setFloat("spotLight.cutOff", glm::cos(glm::radians(12.5f)));
    shader.setFloat("spotLight.outerCutOff", glm::cos(glm::radians(17.5f)));
    shader.setFloat("spotLight.constant", 1.0f);
    shader.setFloat("spotLight.linear", 0.045f);
    shader.setFloat("spotLight.quadratic", 0.015f);
    shader.setVec3("spotLight.ambient", glm::vec3(0.0f));
    if (lighting.isDay) {
        shader.setVec3("spotLight.diffuse", glm::vec3(0.0f));
        shader.setVec3("spotLight.specular", glm::vec3(0.0f));
    } else {
        shader.setVec3("spotLight.diffuse", glm::vec3(0.78f, 0.76f, 0.82f) * lightManager.spotLightStrength);
        shader.setVec3("spotLight.specular", glm::vec3(0.58f, 0.56f, 0.60f) * lightManager.spotLightStrength);
    }
}

void drawSunMarker(DebugAabbDrawer& drawer, GLFWwindow* window, const Camera& camera, const glm::vec3& direction)
{
    int w = 0;
    int h = 0;
    glfwGetFramebufferSize(window, &w, &h);
    if (h <= 0) {
        h = 1;
    }

    const glm::vec3 sunPos = camera.Position - glm::normalize(direction) * 12.0f + glm::vec3(0.0f, 3.0f, 0.0f);
    std::vector<NamedAABB> marker;
    marker.push_back({"Sun", AABB::fromCenterHalfExtents(sunPos, glm::vec3(0.22f))});

    const glm::mat4 proj = glm::perspective(
        glm::radians(camera.Zoom),
        static_cast<float>(w) / static_cast<float>(h), 0.1f, 300.0f);
    drawer.draw(proj, camera.GetViewMatrix(), marker, glm::vec3(1.0f, 0.82f, 0.12f));
}

void alignPointLightsToEmissiveMeshes(LightManager& lightManager, const Scene& scene)
{
    std::vector<EmissiveMeshInfo> emitters = scene.emissiveMeshCenters();
    emitters.erase(std::remove_if(emitters.begin(), emitters.end(), [](const EmissiveMeshInfo& emitter) {
        return glm::dot(emitter.color, emitter.color) < 0.25f;
    }), emitters.end());

    if (emitters.size() < lightManager.pointLights.size()) {
        std::cerr << "[Light] Emissive lamp alignment skipped: found " << emitters.size()
                  << " emissive mesh center(s), need " << lightManager.pointLights.size() << ".\n";
        return;
    }

    std::vector<bool> used(emitters.size(), false);
    for (PointLight& light : lightManager.pointLights) {
        int bestIndex = -1;
        float bestDistance2 = std::numeric_limits<float>::max();
        for (std::size_t i = 0; i < emitters.size(); ++i) {
            if (used[i]) {
                continue;
            }

            const glm::vec3 delta = emitters[i].center - light.position;
            const float distance2 = glm::dot(delta, delta);
            if (distance2 < bestDistance2) {
                bestDistance2 = distance2;
                bestIndex = static_cast<int>(i);
            }
        }

        if (bestIndex >= 0) {
            used[static_cast<std::size_t>(bestIndex)] = true;
            light.position = emitters[static_cast<std::size_t>(bestIndex)].center;
            std::cout << "[Light] " << (light.name.empty() ? "PointLight" : light.name)
                      << " position = (" << light.position.x << ", "
                      << light.position.y << ", " << light.position.z << ")\n";
        }
    }

    std::cout << "[Light] Aligned " << lightManager.pointLights.size()
              << " point light position(s) to emissive lamp mesh centers.\n";
}

void shutdownImGui()
{
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();
}

} // namespace

int main()
{
    AppConfig appCfg;
    appCfg.width = 1280;
    appCfg.height = 720;
    appCfg.title = "Classroom Renderer";
    appCfg.cameraPos = glm::vec3(0.0f, 2.0f, 12.0f);
    appCfg.cameraFov = 45.0f;
    appCfg.cameraSpeed = 5.0f;
    appCfg.cameraSensitivity = 0.06f;

    Application app(appCfg);
    if (!app.window()) {
        return -1;
    }

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGui::StyleColorsDark();
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    ImGui_ImplGlfw_InitForOpenGL(app.window(), true);
    ImGui_ImplOpenGL3_Init("#version 330");

    const std::string vertPath = resolvePath({"shaders/model.vert"});
    const std::string fragPath = resolvePath({"shaders/model.frag"});
    const std::string depthVertPath = resolvePath({"shaders/depth.vert"});
    const std::string depthFragPath = resolvePath({"shaders/depth.frag"});
    const std::string dbgVert = resolvePath({"shaders/debug_line.vert"});
    const std::string dbgFrag = resolvePath({"shaders/debug_line.frag"});
    const std::string lightingConfigPath = resolvePath({"config/lighting_config.json"});
    if (vertPath.empty() || fragPath.empty() || depthVertPath.empty() || depthFragPath.empty()
        || dbgVert.empty() || dbgFrag.empty()) {
        std::cerr << "Required shader files not found.\n";
        shutdownImGui();
        return -1;
    }

    Shader shader(vertPath.c_str(), fragPath.c_str());
    Shader depthShader(depthVertPath.c_str(), depthFragPath.c_str());
    DebugAabbDrawer debugDrawer(Shader(dbgVert.c_str(), dbgFrag.c_str()));
    if (!shader.isValid() || !depthShader.isValid() || !debugDrawer.shader().isValid()) {
        std::cerr << "Shader compilation failed.\n";
        shutdownImGui();
        return -1;
    }

    LightManager lightManager;
    if (lightingConfigPath.empty() || !lightManager.loadConfig(lightingConfigPath)) {
        std::cerr << "[Light] Using built-in fallback lighting values.\n";
    }

    const std::string modelPath = resolvePath({
        "resources/models/partyCarriage.glb",
        // "resources/models/oldschool.glb",
        // "resources/models/oldschool2.glb",
        // "resources/models/oldschool.obj",
        // "resources/models/Tsukinomori.obj",
        // "resources/models/Tsukinomori.fbx",
    });

    Scene scene;
    if (!modelPath.empty()) {
        scene.addModel({modelPath, Transform{glm::vec3(0.0f, 0.0f, 0.0f)}, "Model"});
    }

    if (scene.entries().empty()) {
        std::cerr << "No models in scene.\n";
        shutdownImGui();
        return -1;
    }

    std::string loadStageText = "Preparing...";
    float loadOverall = 0.0f;
    const auto pumpLoadUi = [&](float normalized, const char* status) {
        if (status != nullptr) {
            loadStageText = status;
        }
        loadOverall = std::clamp(normalized, 0.0f, 1.0f);

        glClearColor(0.08f, 0.08f, 0.10f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        ImGuiIO& loadIo = ImGui::GetIO();
        if (loadIo.DisplaySize.x > 0.0f && loadIo.DisplaySize.y > 0.0f) {
            ImGui::SetNextWindowPos(ImVec2(0.0f, 0.0f));
            ImGui::SetNextWindowSize(loadIo.DisplaySize);
            ImGui::Begin("##loading_overlay", nullptr,
                         ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove
                             | ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoNav);
            ImGui::TextUnformatted("Classroom Renderer");
            ImGui::Separator();
            ImGui::TextUnformatted("Loading assets - large models may take one to two minutes.");
            ImGui::ProgressBar(loadOverall, ImVec2(-1.0f, 28.0f));
            ImGui::TextWrapped("%s", loadStageText.c_str());
            ImGui::End();
        }

        ImGui::Render();
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
        restoreGlStateForScene(app.window());
        glfwSwapBuffers(app.window());
        glfwPollEvents();
    };

    std::cout << "[Main] Loading " << scene.entries().size() << " model(s)...\n";
    scene.loadAll(pumpLoadUi);
    alignPointLightsToEmissiveMeshes(lightManager, scene);

    glm::vec3 bmin;
    glm::vec3 bmax;
    if (scene.computeWorldBounds(bmin, bmax)) {
        const glm::vec3 center = 0.5f * (bmin + bmax);
        const float radius = 0.5f * glm::length(bmax - bmin);
        app.camera().fitFreeFlyViewAroundCenter(center, radius);
    }

    Renderer renderer(shader, depthShader, app.camera());
    renderer.setLightManager(&lightManager);
    renderer.setLightDirection(glm::normalize(lightManager.sunLight.direction));

    LightingState lighting;
    bool showDemoColliders = false;
    bool useSceneEntryAABBs = true;
    bool collisionEnabled = false;
    bool drawColliders = false;
    bool showSunMarker = true;
    float shadowStrength = 0.94f;
    glm::vec3 debugColor{0.25f, 0.95f, 0.35f};
    glm::vec3 lastCameraPosition = app.camera().Position;

    app.run([&](float /*dt*/) {
        handleLightingHotkeys(app.window(), lighting);

        if (lighting.isDay) {
            glClearColor(0.52f, 0.74f, 0.93f, 1.0f);
        } else {
            glClearColor(0.02f, 0.02f, 0.05f, 1.0f);
        }
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        restoreGlStateForScene(app.window());

        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();
        app.setInputBlockedByGui(io.WantCaptureMouse || io.WantCaptureKeyboard);

        ImGui::SetNextWindowPos(ImVec2(18.0f, 18.0f), ImGuiCond_Once);
        ImGui::SetNextWindowSize(ImVec2(420.0f, 360.0f), ImGuiCond_Once);
        ImGui::Begin("Lighting Panel");
        ImGui::SetWindowFontScale(1.35f);
        ImGui::Text("FPS %.1f", static_cast<double>(io.Framerate));
        ImGui::Separator();
        ImGui::Checkbox("Point Lights", &lighting.pointLightsOn);
        ImGui::Checkbox("Show Sun Marker", &showSunMarker);
        ImGui::Checkbox("Collision", &collisionEnabled);
        ImGui::Checkbox("Show Colliders", &drawColliders);
        ImGui::Separator();
        ImGui::TextUnformatted("Ambient Light");
        ImGui::ColorEdit3("Ambient Color", &lightManager.globalAmbient[0]);
        ImGui::Separator();
        ImGui::TextUnformatted("Light Intensity");
        ImGui::SliderFloat("Sun Strength", &lightManager.directionalStrength, 0.0f, 3.0f);
        ImGui::SliderFloat("Shadow Strength", &shadowStrength, 0.0f, 1.0f);
        ImGui::SliderFloat("Point Strength", &lightManager.pointLightStrength, 0.0f, 4.0f);
        ImGui::SliderFloat("Flashlight Strength", &lightManager.spotLightStrength, 0.0f, 3.0f);
        ImGui::DragFloat3("Sun Direction", &lightManager.sunLight.direction[0], 0.02f, -1.0f, 1.0f);
        if (glm::dot(lightManager.sunLight.direction, lightManager.sunLight.direction) < 0.0001f) {
            lightManager.sunLight.direction = glm::vec3(-0.2f, -1.0f, -0.3f);
        }
        if (ImGui::Button("Save Lighting Config", ImVec2(-1.0f, 42.0f)) && !lightingConfigPath.empty()) {
            lightManager.saveConfig(lightingConfigPath);
        }
        ImGui::End();

        std::vector<NamedAABB> colliders;
        if (showDemoColliders) {
            appendDemoRoomColliders(colliders);
        }
        if (useSceneEntryAABBs) {
            std::vector<NamedAABB> fromScene = scene.namedWorldAABBs();
            colliders.insert(colliders.end(), fromScene.begin(), fromScene.end());
        }

        Camera& cam = app.camera();
        if (collisionEnabled) {
            resolveCameraCollision(cam, lastCameraPosition, colliders);
        }
        lastCameraPosition = cam.Position;

        renderer.setLightDirection(glm::normalize(lightManager.sunLight.direction));
        renderer.setShadowStrength(shadowStrength);
        syncLightingUniforms(shader, renderer, cam, lighting, lightManager);
        renderer.render(scene);
        if (showSunMarker) {
            drawSunMarker(debugDrawer, app.window(), cam, lightManager.sunLight.direction);
        }

        if (drawColliders && !colliders.empty()) {
            int w = 0;
            int h = 0;
            glfwGetFramebufferSize(app.window(), &w, &h);
            if (h <= 0) {
                h = 1;
            }
            const glm::mat4 proj = glm::perspective(
                glm::radians(cam.Zoom),
                static_cast<float>(w) / static_cast<float>(h), 0.1f, 300.0f);
            const glm::mat4 view = cam.GetViewMatrix();
            debugDrawer.draw(proj, view, colliders, debugColor);
        }

        ImGui::Render();
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
        restoreGlStateForScene(app.window());
    });

    shutdownImGui();
    return 0;
}
