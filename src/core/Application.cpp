#include "core/Application.h"
#include <iostream>

Application::Application(const AppConfig& cfg)
    : camera_(cfg.cameraPos), cfg_(cfg)
{
    if (!glfwInit()) {
        std::cerr << "[App] GLFW init failed.\n";
        return;
    }
    glfwInitialized_ = true;

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
#ifdef __APPLE__
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
#endif
    glfwWindowHint(GLFW_SAMPLES, 2);

    window_ = glfwCreateWindow(cfg_.width, cfg_.height, cfg_.title.c_str(), nullptr, nullptr);
    if (!window_) {
        std::cerr << "[App] Window creation failed.\n";
        glfwTerminate();
        glfwInitialized_ = false;
        return;
    }

    glfwMakeContextCurrent(window_);

    if (!gladLoadGLLoader(reinterpret_cast<GLADloadproc>(glfwGetProcAddress))) {
        std::cerr << "[App] GLAD init failed.\n";
        glfwDestroyWindow(window_);
        glfwTerminate();
        window_ = nullptr;
        glfwInitialized_ = false;
        return;
    }

    glEnable(GL_DEPTH_TEST);
    glEnable(GL_MULTISAMPLE);
    glViewport(0, 0, cfg_.width, cfg_.height);

    camera_.MovementSpeed = cfg_.cameraSpeed;
    camera_.MouseSensitivity = cfg_.cameraSensitivity;
    camera_.Zoom = cfg_.cameraFov;

    lastMouseX_ = static_cast<float>(cfg_.width) * 0.5f;
    lastMouseY_ = static_cast<float>(cfg_.height) * 0.5f;

    glfwSetWindowUserPointer(window_, this);
    glfwSetCursorPosCallback(window_, cursorPosCallback);
    glfwSetScrollCallback(window_, scrollCallback);
    glfwSetInputMode(window_, GLFW_CURSOR, GLFW_CURSOR_NORMAL);

    std::cout << "========================================\n"
              << "  Hold mouse drag : rotate view around the camera\n"
              << "  Tab             : toggle mouse-look lock\n"
              << "  WASD            : move on the horizontal plane\n"
              << "  Space/Shift     : move up / down\n"
              << "  Scroll          : adjust FOV\n"
              << "  1/2             : day / night lighting\n"
              << "  O/C             : open / close classroom lights at night\n"
              << "  Esc             : exit\n"
              << "  (Startup: camera is placed to view the loaded model.)\n"
              << "========================================\n";
}

Application::~Application()
{
    if (window_) {
        glfwDestroyWindow(window_);
        window_ = nullptr;
    }
    if (glfwInitialized_) {
        glfwTerminate();
        glfwInitialized_ = false;
    }
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

    if (!inputBlockedByGui_) {
        if (glfwGetKey(window_, GLFW_KEY_W) == GLFW_PRESS) camera_.ProcessKeyboard(Camera_Movement::FORWARD, deltaTime_);
        if (glfwGetKey(window_, GLFW_KEY_S) == GLFW_PRESS) camera_.ProcessKeyboard(Camera_Movement::BACKWARD, deltaTime_);
        if (glfwGetKey(window_, GLFW_KEY_A) == GLFW_PRESS) camera_.ProcessKeyboard(Camera_Movement::LEFT, deltaTime_);
        if (glfwGetKey(window_, GLFW_KEY_D) == GLFW_PRESS) camera_.ProcessKeyboard(Camera_Movement::RIGHT, deltaTime_);
        if (glfwGetKey(window_, GLFW_KEY_SPACE) == GLFW_PRESS) camera_.ProcessKeyboard(Camera_Movement::UP, deltaTime_);
        if (glfwGetKey(window_, GLFW_KEY_LEFT_SHIFT) == GLFW_PRESS) camera_.ProcessKeyboard(Camera_Movement::DOWN, deltaTime_);
    }

    bool tabNow = (glfwGetKey(window_, GLFW_KEY_TAB) == GLFW_PRESS);
    if (tabNow && !tabWasDown_) {
        cursorLocked_ = !cursorLocked_;
        glfwSetInputMode(window_, GLFW_CURSOR, cursorLocked_ ? GLFW_CURSOR_DISABLED : GLFW_CURSOR_NORMAL);
        firstMouse_ = true;
        std::cout << "[App] Cursor " << (cursorLocked_ ? "locked (Tab)" : "released") << ".\n";
    }
    tabWasDown_ = tabNow;

    const bool leftHeld = glfwGetMouseButton(window_, GLFW_MOUSE_BUTTON_LEFT) == GLFW_PRESS;
    const bool rightHeld = glfwGetMouseButton(window_, GLFW_MOUSE_BUTTON_RIGHT) == GLFW_PRESS;
    const bool shouldCapture = !inputBlockedByGui_ && (leftHeld || rightHeld || cursorLocked_);

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

    if (scrollOffset_ != 0.0f && !inputBlockedByGui_) {
        camera_.ProcessZoom(scrollOffset_);
        scrollOffset_ = 0.0f;
    } else if (inputBlockedByGui_) {
        scrollOffset_ = 0.0f;
    }
}

void Application::cursorPosCallback(GLFWwindow* window, double xpos, double ypos)
{
    auto* app = static_cast<Application*>(glfwGetWindowUserPointer(window));
    if (!app || !app->cursorCaptured_ || app->inputBlockedByGui_) return;

    float x = static_cast<float>(xpos);
    float y = static_cast<float>(ypos);
    if (app->firstMouse_) {
        app->lastMouseX_ = x;
        app->lastMouseY_ = y;
        app->firstMouse_ = false;
        return;
    }

    float dx = x - app->lastMouseX_;
    float dy = y - app->lastMouseY_;
    app->lastMouseX_ = x;
    app->lastMouseY_ = y;

    app->camera_.ProcessMouseMovement(dx, -dy);
}

void Application::scrollCallback(GLFWwindow* window, double /*x*/, double y)
{
    auto* app = static_cast<Application*>(glfwGetWindowUserPointer(window));
    if (app) app->scrollOffset_ += static_cast<float>(y);
}
