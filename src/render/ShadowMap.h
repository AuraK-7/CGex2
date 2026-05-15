#pragma once

#include <glad/glad.h>
#include <glm/mat4x4.hpp>
#include <glm/vec3.hpp>

class ShadowMap {
public:
    ShadowMap(int width, int height);
    ~ShadowMap();

    ShadowMap(const ShadowMap&) = delete;
    ShadowMap& operator=(const ShadowMap&) = delete;

    int width() const { return width_; }
    int height() const { return height_; }
    GLuint depthTexture() const { return depthTexture_; }

    void bindWrite();
    void bindRead(GLuint textureUnit);

    static glm::mat4 computeLightSpaceMatrix(const glm::vec3& sceneMin, const glm::vec3& sceneMax,
        const glm::vec3& lightDirectionUniform);

private:
    int width_;
    int height_;
    GLuint fbo_ = 0;
    GLuint depthTexture_ = 0;
};
