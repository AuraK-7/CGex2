#include <glad/glad.h>
#include <GLFW/glfw3.h>

#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_opengl3.h>

#include "collision/AABB.h"
#include "collision/DebugAabbDrawer.h"
#include "core/Application.h"
#include "core/Camera.h"
#include "render/Renderer.h"
#include "render/Shader.h"
#include "scene/Scene.h"

#include <filesystem>
#include <algorithm>
#include <glm/gtc/matrix_transform.hpp>
#include <iostream>
#include <string>
#include <vector>

namespace {

/// Dear ImGui's OpenGL3 backend may leave depth/blend/viewport/textures changed.
void restoreGlStateForScene(GLFWwindow* window)
{
    int w = 0, h = 0;
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
        std::filesystem::path p(candidate);
        if (p.is_absolute() && std::filesystem::exists(p))
            return std::filesystem::weakly_canonical(p).string();
    }
    for (const auto& root : roots) {
        for (const auto& candidate : candidates) {
            auto full = root / candidate;
            if (std::filesystem::exists(full))
                return std::filesystem::weakly_canonical(full).string();
        }
    }
    return {};
}

bool cameraIntersectsColliders(Camera& camera, const std::vector<NamedAABB>& colliders)
{
    const glm::vec3 bodyCenter = camera.Position - glm::vec3(0.0f, 0.85f, 0.0f);
    const AABB player = AABB::fromCenterHalfExtents(bodyCenter, glm::vec3(0.28f, 0.92f, 0.28f));
    for (const NamedAABB& c : colliders) {
        if (player.intersects(c.box)) return true;
    }
    return false;
}

} // namespace

int main()
{
    Application::Config appCfg;
    appCfg.width = 1280; appCfg.height = 720;
    appCfg.title = "Classroom Renderer";
    appCfg.cameraPos = glm::vec3(0.0f, 2.0f, 12.0f);
    appCfg.cameraFov = 45.0f; appCfg.cameraSpeed = 5.0f; appCfg.cameraSensitivity = 0.15f;
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
    if (vertPath.empty() || fragPath.empty()) {
        std::cerr << "Shader files not found.\n";
        ImGui_ImplOpenGL3_Shutdown();
        ImGui_ImplGlfw_Shutdown();
        ImGui::DestroyContext();
        return -1;
    }
    Shader shader(vertPath.c_str(), fragPath.c_str());
    if (!shader.isValid()) {
        std::cerr << "Shader compilation failed.\n";
        ImGui_ImplOpenGL3_Shutdown();
        ImGui_ImplGlfw_Shutdown();
        ImGui::DestroyContext();
        return -1;
    }

    const std::string dbgVert = resolvePath({"shaders/debug_line.vert"});
    const std::string dbgFrag = resolvePath({"shaders/debug_line.frag"});
    if (dbgVert.empty() || dbgFrag.empty()) {
        std::cerr << "Debug line shader files not found.\n";
        ImGui_ImplOpenGL3_Shutdown();
        ImGui_ImplGlfw_Shutdown();
        ImGui::DestroyContext();
        return -1;
    }
    DebugAabbDrawer debugDrawer(Shader(dbgVert.c_str(), dbgFrag.c_str()));
    if (!debugDrawer.shader().isValid()) {
        std::cerr << "Debug line shader compilation failed.\n";
        ImGui_ImplOpenGL3_Shutdown();
        ImGui_ImplGlfw_Shutdown();
        ImGui::DestroyContext();
        return -1;
    }

    Scene scene;

    const std::string fallbackTex = resolvePath({
        "resources/textures/Wood066_1K-PNG_Color.png",
        "resources/textures/blenderkit_logo.png",
    });

    const std::string modelPath = resolvePath({
        "resources/models/Tsukinomori.obj",
        "resources/models/model.fbx",
    });

    if (!modelPath.empty())
        scene.addModel({modelPath, Transform{glm::vec3(0.0f, 0.0f, 0.0f)}, "Model"});

    if (scene.entries().empty()) {
        std::cerr << "No models in scene.\n";
        ImGui_ImplOpenGL3_Shutdown();
        ImGui_ImplGlfw_Shutdown();
        ImGui::DestroyContext();
        return -1;
    }
    std::cout << "[Main] Loading " << scene.entries().size() << " model(s)...\n";

    std::string loadStageText = "Preparing…";
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

        ImGuiIO& io = ImGui::GetIO();
        if (io.DisplaySize.x > 0.0f && io.DisplaySize.y > 0.0f) {
            ImGui::SetNextWindowPos(ImVec2(0.0f, 0.0f));
            ImGui::SetNextWindowSize(io.DisplaySize);
            ImGui::Begin("##loading_overlay", nullptr,
                          ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove
                              | ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoNav);
            ImGui::TextUnformatted("Classroom Renderer");
            ImGui::Separator();
            ImGui::TextUnformatted("Loading assets — large models may take one to two minutes.");
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

    scene.loadAll(fallbackTex, pumpLoadUi);

    Renderer renderer(shader, app.camera());
    renderer.setLightDirection(glm::vec3(-0.8f, -1.0f, -0.3f));
    renderer.setFallbackColor(glm::vec3(0.8f, 0.8f, 0.8f));

    bool showDemoColliders = true;
    bool useSceneEntryAABBs = false;
    bool collisionEnabled = false;
    bool drawColliders = false;
    bool showImGuiDemo = false;
    glm::vec3 debugColor{0.25f, 0.95f, 0.35f};
    glm::vec3 lastValidCamPos = app.camera().Position;

    app.run([&](float /*dt*/) {
        restoreGlStateForScene(app.window());

        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        ImGui::Begin("Interaction & collision");
        ImGui::Text("FPS: %.1f", static_cast<double>(io.Framerate));
        ImGui::Separator();
        ImGui::Checkbox("Enable FPS collision", &collisionEnabled);
        ImGui::Checkbox("Draw collider AABBs", &drawColliders);
        ImGui::Checkbox("Include demo room walls", &showDemoColliders);
        ImGui::Checkbox("Add scene entry world AABBs", &useSceneEntryAABBs);
        ImGui::ColorEdit3("Debug line color", &debugColor.x);
        ImGui::Checkbox("ImGui demo window", &showImGuiDemo);
        ImGui::Separator();
        ImGui::TextWrapped(
            "Single merged classroom mesh has one huge world AABB; it is a poor wall collider. "
            "Use demo walls for gameplay, or split furniture into separate scene.addModel entries.");
        ImGui::End();

        if (showImGuiDemo) {
            ImGui::ShowDemoWindow(&showImGuiDemo);
        }

        std::vector<NamedAABB> colliders;
        if (showDemoColliders) {
            appendDemoRoomColliders(colliders);
        }
        if (useSceneEntryAABBs) {
            std::vector<NamedAABB> fromScene = scene.namedWorldAABBs();
            colliders.insert(colliders.end(), fromScene.begin(), fromScene.end());
        }

        Camera& cam = app.camera();
        if (collisionEnabled && cam.Mode == CameraMode::FirstPerson && !colliders.empty()) {
            if (cameraIntersectsColliders(cam, colliders)) {
                cam.Position = lastValidCamPos;
            } else {
                lastValidCamPos = cam.Position;
            }
        } else {
            lastValidCamPos = cam.Position;
        }

        renderer.render(scene);

        if (drawColliders && !colliders.empty()) {
            int w = 0, h = 0;
            glfwGetFramebufferSize(app.window(), &w, &h);
            if (h <= 0) h = 1;
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

    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();
    return 0;
}
