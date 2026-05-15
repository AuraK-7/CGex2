#pragma once

#include "Camera.h"
#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <functional>
#include <string>

class Application {
public:
    struct Config {
        int width = 1280, height = 720;
        std::string title = "Classroom Renderer";
        glm::vec3 cameraPos{0.0f, 1.5f, 5.0f};
        float cameraFov = 45.0f, cameraSpeed = 5.0f, cameraSensitivity = 0.15f;
    };

    Application();
    explicit Application(const Config& cfg);
    ~Application();

    Application(const Application&) = delete;
    Application& operator=(const Application&) = delete;

    void run(std::function<void(float deltaTime)> renderFn);

    GLFWwindow* window() const { return window_; }
    Camera& camera() { return camera_; }
    const Camera& camera() const { return camera_; }

private:
    GLFWwindow* window_ = nullptr;
    Camera camera_;
    Config cfg_;
    float deltaTime_ = 0.0f, lastFrame_ = 0.0f;

    bool cursorCaptured_ = false, cursorLocked_ = false;
    bool tabWasDown_ = false, fWasDown_ = false;
    float lastMouseX_ = 0.0f, lastMouseY_ = 0.0f;
    bool firstMouse_ = true;
    float scrollOffset_ = 0.0f;

    void processInput();

    static void cursorPosCallback(GLFWwindow* window, double x, double y);
    static void scrollCallback(GLFWwindow* window, double x, double y);
};
