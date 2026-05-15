#include "render/ShadowMap.h"

#include <glm/gtc/matrix_transform.hpp>

#include <algorithm>
#include <array>
#include <cmath>
#include <iostream>

ShadowMap::ShadowMap(int width, int height)
    : width_(width)
    , height_(height)
{
    glGenFramebuffers(1, &fbo_);
    glGenTextures(1, &depthTexture_);
    glBindTexture(GL_TEXTURE_2D, depthTexture_);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT, width_, height_, 0, GL_DEPTH_COMPONENT, GL_FLOAT, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);
    const float border[] = {1.0f, 1.0f, 1.0f, 1.0f};
    glTexParameterfv(GL_TEXTURE_2D, GL_TEXTURE_BORDER_COLOR, border);

    glBindFramebuffer(GL_FRAMEBUFFER, fbo_);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, depthTexture_, 0);
    glDrawBuffer(GL_NONE);
    glReadBuffer(GL_NONE);
    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
        std::cerr << "[ShadowMap] Framebuffer incomplete.\n";
    }
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

ShadowMap::~ShadowMap()
{
    if (depthTexture_ != 0) {
        glDeleteTextures(1, &depthTexture_);
    }
    if (fbo_ != 0) {
        glDeleteFramebuffers(1, &fbo_);
    }
}

void ShadowMap::bindWrite()
{
    glBindFramebuffer(GL_FRAMEBUFFER, fbo_);
    glViewport(0, 0, width_, height_);
    glClear(GL_DEPTH_BUFFER_BIT);
}

void ShadowMap::bindRead(GLuint textureUnit)
{
    glActiveTexture(GL_TEXTURE0 + textureUnit);
    glBindTexture(GL_TEXTURE_2D, depthTexture_);
}

glm::mat4 ShadowMap::computeLightSpaceMatrix(const glm::vec3& sceneMin, const glm::vec3& sceneMax,
    const glm::vec3& lightDirectionUniform)
{
    const glm::vec3 center = 0.5f * (sceneMin + sceneMax);
    const glm::vec3 d = glm::normalize(lightDirectionUniform);
    const float extent = glm::length(sceneMax - sceneMin);
    const float dist = std::max(extent * 0.65f, 6.0f);

    const glm::vec3 eye = center + d * dist;
    glm::mat4 lightView = glm::lookAt(eye, center, glm::vec3(0.0f, 1.0f, 0.0f));

    const std::array<glm::vec3, 8> corners = {
        glm::vec3(sceneMin.x, sceneMin.y, sceneMin.z),
        glm::vec3(sceneMax.x, sceneMin.y, sceneMin.z),
        glm::vec3(sceneMin.x, sceneMax.y, sceneMin.z),
        glm::vec3(sceneMax.x, sceneMax.y, sceneMin.z),
        glm::vec3(sceneMin.x, sceneMin.y, sceneMax.z),
        glm::vec3(sceneMax.x, sceneMin.y, sceneMax.z),
        glm::vec3(sceneMin.x, sceneMax.y, sceneMax.z),
        glm::vec3(sceneMax.x, sceneMax.y, sceneMax.z),
    };

    float minX = 1e10f, maxX = -1e10f, minY = 1e10f, maxY = -1e10f, minZ = 1e10f, maxZ = -1e10f;
    for (const glm::vec3& c : corners) {
        const glm::vec4 tr = lightView * glm::vec4(c, 1.0f);
        minX = std::min(minX, tr.x);
        maxX = std::max(maxX, tr.x);
        minY = std::min(minY, tr.y);
        maxY = std::max(maxY, tr.y);
        minZ = std::min(minZ, tr.z);
        maxZ = std::max(maxZ, tr.z);
    }

    const float padXY = std::max(extent * 0.08f, 1.5f);
    const float padZ = std::max((maxZ - minZ) * 0.35f, 2.0f);
    float zNear = minZ - padZ;
    float zFar = maxZ + padZ;
    if (zNear > zFar) {
        std::swap(zNear, zFar);
    }
    const glm::mat4 lightProj = glm::ortho(minX - padXY, maxX + padXY, minY - padXY, maxY + padXY,
        zNear, zFar);

    return lightProj * lightView;
}
