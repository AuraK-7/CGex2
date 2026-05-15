#include "core/Application.h"
#include <iostream>

Application::Application(const AppConfig& cfg)
    : camera_(cfg.cameraPos), cfg_(cfg)
{
    if (!glfwInit()) {
        std::cerr << "[App] GLFW init failed.\n";
        return;
    }

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
#ifdef __APPLE__
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
#endif
    glfwWindowHint(GLFW_SAMPLES, 2);

    window_ = glfwCreateWindow(cfg_.width, cfg_.height, cfg_.title.c_str(), nullptr, nullptr);
    if (!window_) { std::cerr << "[App] Window creation failed.\n"; glfwTerminate(); return; }

    glfwMakeContextCurrent(window_);

    if (!gladLoadGLLoader(reinterpret_cast<GLADloadproc>(glfwGetProcAddress))) {
        std::cerr << "[App] GLAD init failed.\n";
        glfwDestroyWindow(window_); glfwTerminate(); window_ = nullptr; return;
    }

    glEnable(GL_DEPTH_TEST);
    glEnable(GL_MULTISAMPLE);
    glViewport(0, 0, cfg_.width, cfg_.height);

    camera_.MovementSpeed = cfg_.cameraSpeed;
    camera_.MouseSensitivity = cfg_.cameraSensitivity;
    camera_.Zoom = cfg_.cameraFov;
    camera_.OrbitTarget = cfg_.cameraPos + glm::vec3(0.0f, 0.0f, -5.0f);
    camera_.OrbitDistance = 5.0f;

    lastMouseX_ = static_cast<float>(cfg_.width) * 0.5f;
    lastMouseY_ = static_cast<float>(cfg_.height) * 0.5f;

    glfwSetWindowUserPointer(window_, this);
    glfwSetCursorPosCallback(window_, cursorPosCallback);
    glfwSetScrollCallback(window_, scrollCallback);
    glfwSetInputMode(window_, GLFW_CURSOR, GLFW_CURSOR_NORMAL);

    std::cout << "========================================\n"
              << "  Left/Right drag : look / orbit\n"
              << "  WASD            : move (FPS) / pan target (orbit)\n"
              << "  F               : toggle FPS / Orbit camera\n"
              << "  Tab             : lock cursor (FPS)\n"
              << "  Scroll          : zoom\n"
              << "  Esc             : exit\n"
              << "  (Startup: orbit camera is fitted to the model bounds.)\n"
              << "========================================\n";
}

Application::~Application()
{
    if (window_) { glfwDestroyWindow(window_); }
    glfwTerminate();
}

void Application::run(std::function<void(float)> renderFn)
{
    if (!window_ || !renderFn) return;
    lastFrame_ = static_cast<float>(glfwGetTime());
    while (!glfwWindowShouldClose(window_)) {
        float now = static_cast<float>(glfwGetTime());
        deltaTime_ = now - lastFrame_;
        lastFrame_ = now;
        processInput();
        renderFn(deltaTime_);
        glfwSwapBuffers(window_);
        glfwPollEvents();
    }
}

void Application::processInput()
{
    if (glfwGetKey(window_, GLFW_KEY_ESCAPE) == GLFW_PRESS)
        glfwSetWindowShouldClose(window_, GLFW_TRUE);

    bool fNow = (glfwGetKey(window_, GLFW_KEY_F) == GLFW_PRESS);
    if (fNow && !fWasDown_) {
        camera_.SetMode(camera_.Mode == CameraMode::FirstPerson
                        ? CameraMode::Orbit : CameraMode::FirstPerson);
        std::cout << "[App] Camera: " << (camera_.Mode == CameraMode::Orbit ? "Orbit" : "FPS") << "\n";
    }
    fWasDown_ = fNow;

    if (glfwGetKey(window_, GLFW_KEY_W) == GLFW_PRESS) camera_.ProcessKeyboard(Camera_Movement::FORWARD, deltaTime_);
    if (glfwGetKey(window_, GLFW_KEY_S) == GLFW_PRESS) camera_.ProcessKeyboard(Camera_Movement::BACKWARD, deltaTime_);
    if (glfwGetKey(window_, GLFW_KEY_A) == GLFW_PRESS) camera_.ProcessKeyboard(Camera_Movement::LEFT, deltaTime_);
    if (glfwGetKey(window_, GLFW_KEY_D) == GLFW_PRESS) camera_.ProcessKeyboard(Camera_Movement::RIGHT, deltaTime_);

    bool tabNow = (glfwGetKey(window_, GLFW_KEY_TAB) == GLFW_PRESS);
    if (tabNow && !tabWasDown_) {
        cursorLocked_ = !cursorLocked_;
        glfwSetInputMode(window_, GLFW_CURSOR, cursorLocked_ ? GLFW_CURSOR_DISABLED : GLFW_CURSOR_NORMAL);
        firstMouse_ = true;
        std::cout << "[App] Cursor " << (cursorLocked_ ? "locked (Tab)" : "released") << ".\n";
    }
    tabWasDown_ = tabNow;

    bool leftHeld = glfwGetMouseButton(window_, GLFW_MOUSE_BUTTON_LEFT) == GLFW_PRESS;
    bool rightHeld = glfwGetMouseButton(window_, GLFW_MOUSE_BUTTON_RIGHT) == GLFW_PRESS;
    bool shouldCapture = leftHeld || rightHeld || cursorLocked_;

    if (shouldCapture && !cursorCaptured_) {
        cursorCaptured_ = true;
        glfwSetInputMode(window_, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
        firstMouse_ = true;
    }
    if (!shouldCapture && cursorCaptured_) {
        cursorCaptured_ = false;
        glfwSetInputMode(window_, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
        firstMouse_ = true;
    }

    if (scrollOffset_ != 0.0f) {
        camera_.ProcessZoom(scrollOffset_);
        scrollOffset_ = 0.0f;
    }
}

void Application::cursorPosCallback(GLFWwindow* window, double xpos, double ypos)
{
    auto* app = static_cast<Application*>(glfwGetWindowUserPointer(window));
    if (!app || !app->cursorCaptured_) return;

    float x = static_cast<float>(xpos);
    float y = static_cast<float>(ypos);
    if (app->firstMouse_) { app->lastMouseX_ = x; app->lastMouseY_ = y; app->firstMouse_ = false; return; }

    float dx = x - app->lastMouseX_;
    float dy = y - app->lastMouseY_;
    app->lastMouseX_ = x;
    app->lastMouseY_ = y;

    if (app->camera_.Mode == CameraMode::Orbit)
        app->camera_.ProcessOrbit(dx, dy);
    else
        app->camera_.ProcessMouseMovement(dx, -dy);
}

void Application::scrollCallback(GLFWwindow* window, double /*x*/, double y)
{
    auto* app = static_cast<Application*>(glfwGetWindowUserPointer(window));
    if (app) app->scrollOffset_ += static_cast<float>(y);
}
