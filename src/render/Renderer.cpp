#include "render/Renderer.h"
#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <glm/gtc/matrix_transform.hpp>

#include <string>

namespace {

constexpr int kNumPointLights = 4;

void setPointLightUniforms(Shader& shader, int index, const glm::vec3& position,
    float constantTerm, float linearTerm, float quadraticTerm,
    const glm::vec3& ambient, const glm::vec3& diffuse, const glm::vec3& specular)
{
    const std::string base = "pointLights[" + std::to_string(index) + "]";
    shader.setVec3(base + ".position", position);
    shader.setFloat(base + ".constant", constantTerm);
    shader.setFloat(base + ".linear", linearTerm);
    shader.setFloat(base + ".quadratic", quadraticTerm);
    shader.setVec3(base + ".ambient", ambient);
    shader.setVec3(base + ".diffuse", diffuse);
    shader.setVec3(base + ".specular", specular);
}

} // namespace

Renderer::Renderer(Shader& litShader, Shader& depthShader, Camera& camera)
    : litShader_(litShader)
    , depthShader_(depthShader)
    , camera_(camera)
    , shadowMap_(1024, 1024)
{
}

void Renderer::render(const Scene& scene)
{
    GLint viewport[4] = {0, 0, 0, 0};
    glGetIntegerv(GL_VIEWPORT, viewport);

    if (!sceneBoundsTried_) {
        sceneBoundsValid_ = scene.computeWorldBounds(sceneBmin_, sceneBmax_);
        sceneBoundsTried_ = true;
    }
    const bool haveBounds = sceneBoundsValid_;

    const bool wantShadowPass = directionalShadowsEnabled_ && haveBounds && depthShader_.isValid();

    glm::mat4 lightSpace(1.0f);
    if (wantShadowPass) {
        lightSpace = ShadowMap::computeLightSpaceMatrix(sceneBmin_, sceneBmax_, glm::normalize(lightDir_));
    }

    if (wantShadowPass) {
        glEnable(GL_CULL_FACE);
        glCullFace(GL_FRONT);
        glEnable(GL_POLYGON_OFFSET_FILL);
        glPolygonOffset(1.25f, 3.0f);

        shadowMap_.bindWrite();
        depthShader_.use();
        depthShader_.setMat4("uLightSpaceMatrix", lightSpace);
        scene.drawAll(depthShader_, true);

        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        glCullFace(GL_BACK);
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

    const glm::vec3 ceilingY(0.0f, 2.35f, 0.0f);
    const glm::vec3 corners[4] = {
        glm::vec3(-2.2f, 0.0f, 2.5f),
        glm::vec3(2.2f, 0.0f, 2.5f),
        glm::vec3(-2.2f, 0.0f, 6.5f),
        glm::vec3(2.2f, 0.0f, 6.5f),
    };
    const glm::vec3 plAmb(0.07f, 0.07f, 0.075f);
    const glm::vec3 plDiff(0.74f, 0.69f, 0.58f);
    const glm::vec3 plSpec(0.52f, 0.48f, 0.41f);
    for (int i = 0; i < kNumPointLights; ++i) {
        setPointLightUniforms(litShader_, i, ceilingY + corners[i],
            1.0f, 0.11f, 0.09f, plAmb, plDiff, plSpec);
    }

    scene.drawAll(litShader_, false);
}
