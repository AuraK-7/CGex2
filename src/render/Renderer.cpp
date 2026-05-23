#include "render/Renderer.h"
#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <glm/gtc/matrix_transform.hpp>
#include <iostream>

void Renderer::toggleDayNight()
{
    dayMode_ = !dayMode_;
    if (dayMode_) {
        ambientStrength_ = 0.25f;
        ambientColor_ = glm::vec3(1.0f, 1.0f, 1.0f);
        std::cout << "[Renderer] Day mode\n";
    } else {
        ambientStrength_ = 0.06f;
        ambientColor_ = glm::vec3(0.4f, 0.5f, 0.8f);
        std::cout << "[Renderer] Night mode\n";
    }
}

Renderer::Renderer(Shader& litShader, Shader& depthShader, Camera& camera)
    : litShader_(litShader)
    , depthShader_(depthShader)
    , camera_(camera)
    , shadowMap_(2048, 2048)
{
}

void Renderer::render(const Scene& scene)
{
    static bool printedShadowStatus = false;

    GLint viewport[4] = {0, 0, 0, 0};
    glGetIntegerv(GL_VIEWPORT, viewport);

    if (!sceneBoundsTried_) {
        sceneBoundsValid_ = scene.computeWorldBounds(sceneBmin_, sceneBmax_);
        sceneBoundsTried_ = true;
    }
    const bool haveBounds = sceneBoundsValid_;

    const bool wantShadowPass = directionalShadowsEnabled_ && haveBounds && depthShader_.isValid();
    if (!printedShadowStatus) {
        if (wantShadowPass) {
            std::cout << "[Shadow] Directional shadow mapping enabled. Resolution: "
                      << shadowMap_.width() << "x" << shadowMap_.height() << '\n';
        } else {
            std::cerr << "[Shadow] Directional shadow mapping disabled.\n"
                      << "  directionalShadowsEnabled: " << (directionalShadowsEnabled_ ? "true" : "false") << '\n'
                      << "  sceneBoundsValid: " << (haveBounds ? "true" : "false") << '\n'
                      << "  depthShaderValid: " << (depthShader_.isValid() ? "true" : "false") << '\n';
        }
        printedShadowStatus = true;
    }

    glm::mat4 lightSpace(1.0f);
    if (wantShadowPass) {
        lightSpace = ShadowMap::computeLightSpaceMatrix(sceneBmin_, sceneBmax_, glm::normalize(lightDir_));
    }

    if (wantShadowPass) {
        const GLboolean cullFaceWasEnabled = glIsEnabled(GL_CULL_FACE);
        GLint previousCullFace = GL_BACK;
        glGetIntegerv(GL_CULL_FACE_MODE, &previousCullFace);

        glDisable(GL_CULL_FACE);
        glEnable(GL_POLYGON_OFFSET_FILL);
        glPolygonOffset(1.25f, 3.0f);

        shadowMap_.bindWrite();
        depthShader_.use();
        depthShader_.setMat4("uLightSpaceMatrix", lightSpace);
        scene.drawAll(depthShader_, true);

        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        if (cullFaceWasEnabled) {
            glCullFace(previousCullFace);
        }
        if (cullFaceWasEnabled) {
            glEnable(GL_CULL_FACE);
        } else {
            glDisable(GL_CULL_FACE);
        }
        glDisable(GL_POLYGON_OFFSET_FILL);
        glViewport(viewport[0], viewport[1], viewport[2], viewport[3]);
    }

    litShader_.use();
    int w = viewport[2];
    int h = viewport[3];
    if (w <= 0 || h <= 0) {
        GLFWwindow* win = glfwGetCurrentContext();
        if (win) glfwGetFramebufferSize(win, &w, &h);
    }
    if (h <= 0) h = 1;

    litShader_.setMat4("uProjection", glm::perspective(
        glm::radians(camera_.Zoom),
        static_cast<float>(w) / static_cast<float>(h), 0.1f, 2000.0f));
    litShader_.setMat4("uView", camera_.GetViewMatrix());
    litShader_.setVec3("uViewPosition", camera_.Position);

    const glm::vec3 dir = glm::normalize(lightDir_);
    litShader_.setVec3("dirLight.direction", dir);
    litShader_.setMat4("uLightSpaceMatrix", lightSpace);
    litShader_.setInt("uUseDirectionalShadow", wantShadowPass ? 1 : 0);
    litShader_.setFloat("uShadowStrength", shadowStrength_);
    litShader_.setInt("shadowMap", kShadowMapUnit);
    litShader_.setVec2("uShadowTexelSize", glm::vec2(
        1.0f / static_cast<float>(shadowMap_.width()),
        1.0f / static_cast<float>(shadowMap_.height())));
    if (sceneBoundsValid_) {
        litShader_.setVec3("uSceneMin", sceneBmin_);
        litShader_.setVec3("uSceneMax", sceneBmax_);
    } else {
        litShader_.setVec3("uSceneMin", glm::vec3(-1.0f));
        litShader_.setVec3("uSceneMax", glm::vec3(1.0f));
    }
    litShader_.setInt("uApplyWindowFalloff", directionalShadowsEnabled_ && sceneBoundsValid_ ? 1 : 0);
    shadowMap_.bindRead(static_cast<GLuint>(kShadowMapUnit));

    if (lightManager_ != nullptr) {
        lightManager_->sendToShader(litShader_.id());
    } else {
        litShader_.setVec3("globalAmbient", ambientColor_ * ambientStrength_);
        litShader_.setInt("numPointLights", 0);
    }

    scene.drawAll(litShader_, false, &camera_.Position);
}
