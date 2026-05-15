#include "core/Application.h"
#include "render/Renderer.h"
#include "render/Shader.h"
#include "scene/Scene.h"

#include <glad/glad.h>
#include <GLFW/glfw3.h>

#include <atomic>
#include <filesystem>
#include <iostream>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

#include <glm/geometric.hpp>

namespace {

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

} // namespace

int main()
{
    AppConfig appCfg;
    appCfg.width = 1280;
    appCfg.height = 720;
    appCfg.title = "Classroom Renderer";
    appCfg.cameraPos = glm::vec3(0.0f, 2.0f, 8.0f);
    appCfg.cameraFov = 45.0f;
    appCfg.cameraSpeed = 5.0f;
    appCfg.cameraSensitivity = 0.15f;
    Application app(appCfg);
    if (app.window() == nullptr) {
        return -1;
    }

    const std::string vertPath = resolvePath({"shaders/model.vert"});
    const std::string fragPath = resolvePath({"shaders/model.frag"});
    if (vertPath.empty() || fragPath.empty()) {
        std::cerr << "Shader files not found.\n";
        return -1;
    }
    Shader shader(vertPath.c_str(), fragPath.c_str());
    if (!shader.isValid()) {
        std::cerr << "Shader compilation failed.\n";
        return -1;
    }

    const std::string depthVertPath = resolvePath({"shaders/depth.vert"});
    const std::string depthFragPath = resolvePath({"shaders/depth.frag"});
    Shader depthShader(depthVertPath.c_str(), depthFragPath.c_str());
    if (!depthShader.isValid()) {
        std::cerr << "[Main] Depth shader failed; directional soft shadows disabled.\n";
    }

    const std::string fallbackTex = resolvePath({
        "resources/textures/Wood066_1K-PNG_Color.png",
        "resources/textures/blenderkit_logo.png",
    });

    const std::string modelPath = resolvePath({
        "resources/models/Tsukinomori.obj",
    });

    auto scene = std::make_unique<Scene>();
    if (!modelPath.empty()) {
        scene->addModel({modelPath, Transform{glm::vec3(0.0f, 0.0f, 0.0f)}, "Model"});
    }

    // const std::string deskPath = resolvePath({"resources/models/classroom_desk.glb"});
    // if (!deskPath.empty())
    //     scene->addModel({deskPath, Transform{glm::vec3(5.0f, 0.0f, 0.0f)}, "Desk"});

    if (scene->entries().empty()) {
        std::cerr << "No models in scene.\n";
        return -1;
    }

    Renderer renderer(shader, depthShader, app.camera());
    renderer.setLightDirection(glm::normalize(glm::vec3(-0.52f, 0.40f, 0.58f)));

    shader.use();
    shader.setFloat("uExposure", 1.0f);

    int isPointLightOn = 0;
    bool isDay = true;

    glfwWindowHint(GLFW_VISIBLE, GLFW_FALSE);
    GLFWwindow* loaderWindow = glfwCreateWindow(1, 1, "Asset Loader", nullptr, app.window());
    glfwWindowHint(GLFW_VISIBLE, GLFW_TRUE);
    if (loaderWindow == nullptr) {
        std::cerr << "[Main] Failed to create shared OpenGL context for background loading.\n";
        return -1;
    }

    auto loadingScene = std::move(scene);
    std::unique_ptr<Scene> loadedScene;
    std::mutex loadedSceneMutex;
    std::atomic_bool loadDone = false;
    std::atomic_bool loadFailed = false;

    std::thread loader([loaderWindow, fallbackTex, sceneToLoad = std::move(loadingScene), &loadedScene, &loadedSceneMutex, &loadDone, &loadFailed]() mutable {
        glfwMakeContextCurrent(loaderWindow);
        if (!gladLoadGLLoader(reinterpret_cast<GLADloadproc>(glfwGetProcAddress))) {
            std::cerr << "[Main] Failed to initialize GLAD in loader context.\n";
            loadFailed.store(true, std::memory_order_release);
            loadDone.store(true, std::memory_order_release);
            return;
        }

        std::cout << "[Main] Loading " << sceneToLoad->entries().size() << " model(s) in background...\n";
        sceneToLoad->loadAll(fallbackTex);
        sceneToLoad->releaseVertexArraysForCurrentContext();

        glfwMakeContextCurrent(nullptr);

        {
            std::lock_guard<std::mutex> lock(loadedSceneMutex);
            loadedScene = std::move(sceneToLoad);
            loadFailed.store(loadedScene->modelCount() == 0, std::memory_order_release);
        }
        loadDone.store(true, std::memory_order_release);
    });

    Scene* activeScene = nullptr;
    bool sceneActivated = false;

    app.run([&](float /*dt*/) {
        if (loadDone.load(std::memory_order_acquire) && !sceneActivated) {
            std::lock_guard<std::mutex> lock(loadedSceneMutex);
            if (!loadFailed.load(std::memory_order_acquire) && loadedScene) {
                loadedScene->createVertexArraysForCurrentContext();
                activeScene = loadedScene.get();
                glm::vec3 bmin;
                glm::vec3 bmax;
                if (activeScene->computeWorldBounds(bmin, bmax)) {
                    const glm::vec3 center = 0.5f * (bmin + bmax);
                    const float radius = 0.5f * glm::length(bmax - bmin);
                    app.camera().fitOrbitAroundCenter(center, radius);
                }
                std::cout << "[Main] Background model loading complete.\n";
            } else {
                std::cerr << "[Main] Background model loading failed.\n";
            }
            sceneActivated = true;
        }

        if (activeScene == nullptr) {
            glClearColor(0.08f, 0.08f, 0.10f, 1.0f);
            glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
            return;
        }

        shader.use();

        GLFWwindow* currentWindow = glfwGetCurrentContext();
        if (currentWindow) {
            if (glfwGetKey(currentWindow, GLFW_KEY_1) == GLFW_PRESS) {
                isDay = true;
            }
            if (glfwGetKey(currentWindow, GLFW_KEY_2) == GLFW_PRESS) {
                isDay = false;
            }

            if (glfwGetKey(currentWindow, GLFW_KEY_O) == GLFW_PRESS) {
                isPointLightOn = 1;
            }
            if (glfwGetKey(currentWindow, GLFW_KEY_C) == GLFW_PRESS) {
                isPointLightOn = 0;
            }
        }

        if (isDay) {
            glClearColor(0.52f, 0.74f, 0.93f, 1.0f);
            renderer.setDirectionalShadowsEnabled(true);
            shader.setFloat("uExposure", 1.0f);
            shader.setVec3("dirLight.ambient", glm::vec3(0.10f, 0.105f, 0.12f));
            shader.setVec3("dirLight.diffuse", glm::vec3(0.88f, 0.82f, 0.68f));
            shader.setVec3("dirLight.specular", glm::vec3(0.22f, 0.20f, 0.18f));

            shader.setInt("uPointLightsOn", 0);
        } else {
            glClearColor(0.02f, 0.02f, 0.05f, 1.0f);
            renderer.setDirectionalShadowsEnabled(false);
            shader.setFloat("uExposure", 1.12f);

            shader.setVec3("dirLight.ambient", glm::vec3(0.02f, 0.02f, 0.05f));
            shader.setVec3("dirLight.diffuse", glm::vec3(0.05f, 0.05f, 0.1f));
            shader.setVec3("dirLight.specular", glm::vec3(0.1f, 0.1f, 0.1f));

            shader.setInt("uPointLightsOn", isPointLightOn);
        }

        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        shader.setVec3("uViewPosition", app.camera().Position);

        shader.setVec3("spotLight.position", app.camera().Position);
        shader.setVec3("spotLight.direction", app.camera().Front);
        shader.setFloat("spotLight.cutOff", glm::cos(glm::radians(12.5f)));
        shader.setFloat("spotLight.outerCutOff", glm::cos(glm::radians(17.5f)));
        shader.setFloat("spotLight.constant", 1.0f);
        shader.setFloat("spotLight.linear", 0.045f);
        shader.setFloat("spotLight.quadratic", 0.015f);
        shader.setVec3("spotLight.ambient", glm::vec3(0.0f));
        if (isDay) {
            shader.setVec3("spotLight.diffuse", glm::vec3(0.0f));
            shader.setVec3("spotLight.specular", glm::vec3(0.0f));
        } else {
            shader.setVec3("spotLight.diffuse", glm::vec3(0.78f, 0.76f, 0.82f));
            shader.setVec3("spotLight.specular", glm::vec3(0.58f, 0.56f, 0.60f));
        }

        renderer.render(*activeScene);
    });

    if (loader.joinable()) {
        loader.join();
    }
    glfwMakeContextCurrent(app.window());
    loadedScene.reset();
    glfwDestroyWindow(loaderWindow);

    return 0;
}
