#pragma once

#include <glad/glad.h>
#include <algorithm>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

enum class Camera_Movement { FORWARD, BACKWARD, LEFT, RIGHT };
enum class CameraMode { FirstPerson, Orbit };

constexpr float YAW = -90.0f;
constexpr float PITCH = 0.0f;
constexpr float SPEED = 2.5f;
constexpr float SENSITIVITY = 0.1f;
constexpr float ZOOM = 45.0f;

class Camera {
public:
    glm::vec3 Position, Front, Up, Right, WorldUp;
    float Yaw, Pitch, MovementSpeed, MouseSensitivity, Zoom;

    CameraMode Mode = CameraMode::FirstPerson;
    glm::vec3 OrbitTarget{0.0f};
    float OrbitDistance = 5.0f;

    explicit Camera(glm::vec3 position = glm::vec3(0.0f, 0.0f, 3.0f),
                    glm::vec3 up = glm::vec3(0.0f, 1.0f, 0.0f),
                    float yaw = YAW, float pitch = PITCH)
        : Position(position), Front(glm::vec3(0.0f, 0.0f, -1.0f)), WorldUp(up)
        , Yaw(yaw), Pitch(pitch), MovementSpeed(SPEED)
        , MouseSensitivity(SENSITIVITY), Zoom(ZOOM)
    { updateCameraVectors(); }

    glm::mat4 GetViewMatrix() const { return glm::lookAt(Position, Position + Front, Up); }

    void ProcessKeyboard(Camera_Movement dir, float dt)
    {
        float v = MovementSpeed * dt;
        glm::vec3 f = glm::normalize(glm::vec3(Front.x, 0.0f, Front.z));
        glm::vec3 r = glm::normalize(glm::vec3(Right.x, 0.0f, Right.z));
        if (dir == Camera_Movement::FORWARD)  Position += f * v;
        if (dir == Camera_Movement::BACKWARD) Position -= f * v;
        if (dir == Camera_Movement::LEFT)     Position -= r * v;
        if (dir == Camera_Movement::RIGHT)    Position += r * v;
        if (Mode == CameraMode::Orbit) OrbitTarget = Position + Front * OrbitDistance;
    }

    void ProcessMouseMovement(float xoff, float yoff, bool clamp = true)
    {
        xoff *= MouseSensitivity; yoff *= MouseSensitivity;
        Yaw += xoff; Pitch += yoff;
        if (clamp) { if (Pitch > 89.0f) Pitch = 89.0f; if (Pitch < -89.0f) Pitch = -89.0f; }
        updateCameraVectors();
        if (Mode == CameraMode::Orbit) Position = OrbitTarget - Front * OrbitDistance;
    }

    void ProcessOrbit(float xoff, float yoff)
    {
        xoff *= MouseSensitivity; yoff *= MouseSensitivity;
        Yaw += xoff; Pitch -= yoff;
        if (Pitch > 89.0f) Pitch = 89.0f; if (Pitch < -89.0f) Pitch = -89.0f;
        updateCameraVectors();
        Position = OrbitTarget - Front * OrbitDistance;
    }

    void ProcessZoom(float offset)
    {
        if (Mode == CameraMode::Orbit) {
            OrbitDistance -= offset;
            if (OrbitDistance < 0.5f) OrbitDistance = 0.5f;
            if (OrbitDistance > 200.0f) OrbitDistance = 200.0f;
            Position = OrbitTarget - Front * OrbitDistance;
        } else {
            Zoom -= offset;
            if (Zoom < 1.0f) Zoom = 1.0f;
            if (Zoom > 45.0f) Zoom = 45.0f;
        }
    }

    void SetMode(CameraMode mode)
    {
        if (mode == Mode) return;
        if (mode == CameraMode::Orbit)
            OrbitTarget = Position + Front * OrbitDistance;
        else
            OrbitDistance = glm::distance(Position, OrbitTarget);
        Mode = mode;
    }

    void fitOrbitAroundCenter(const glm::vec3& center, float boundingRadius)
    {
        OrbitTarget = center;
        const float r = std::max(boundingRadius, 0.5f);
        OrbitDistance = std::max(r * 2.0f, 5.0f);
        Yaw = -125.0f;
        Pitch = -18.0f;
        updateCameraVectors();
        Position = OrbitTarget - Front * OrbitDistance;
        Mode = CameraMode::Orbit;
    }

private:
    void updateCameraVectors()
    {
        glm::vec3 f;
        f.x = cos(glm::radians(Yaw)) * cos(glm::radians(Pitch));
        f.y = sin(glm::radians(Pitch));
        f.z = sin(glm::radians(Yaw)) * cos(glm::radians(Pitch));
        Front = glm::normalize(f);
        Right = glm::normalize(glm::cross(Front, WorldUp));
        Up = glm::normalize(glm::cross(Right, Front));
    }
};
