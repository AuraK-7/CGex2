#pragma once

#include "Camera.h"
#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <functional>
#include <string>

struct AppConfig {
    int width = 1280, height = 720;
    std::string title = "Classroom Renderer";
    glm::vec3 cameraPos{0.0f, 1.5f, 5.0f};
    float cameraFov = 45.0f, cameraSpeed = 5.0f, cameraSensitivity = 0.06f;
};

class Application {
public:
    explicit Application(const AppConfig& cfg = {});
    ~Application();

    Application(const Application&) = delete;
    Application& operator=(const Application&) = delete;
    Application(Application&&) = delete;
    Application& operator=(Application&&) = delete;

    void run(std::function<void(float deltaTime)> renderFn);

    GLFWwindow* window() const { return window_; }
    Camera& camera() { return camera_; }
    const Camera& camera() const { return camera_; }
    void setInputBlockedByGui(bool blocked) { inputBlockedByGui_ = blocked; }

private:
    GLFWwindow* window_ = nullptr;
    bool glfwInitialized_ = false;
    Camera camera_;
    AppConfig cfg_;
    float deltaTime_ = 0.0f, lastFrame_ = 0.0f;

    bool cursorCaptured_ = false, cursorLocked_ = false;
    bool inputBlockedByGui_ = false;
    bool tabWasDown_ = false;
    float lastMouseX_ = 0.0f, lastMouseY_ = 0.0f;
    bool firstMouse_ = true;
    float scrollOffset_ = 0.0f;

    void processInput();

    static void cursorPosCallback(GLFWwindow* window, double x, double y);
    static void scrollCallback(GLFWwindow* window, double x, double y);
};
