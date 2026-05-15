#define STB_IMAGE_IMPLEMENTATION
#include "Texture.h"

#include <filesystem>
#include <fstream>
#include <iostream>
#include <vector>

namespace {

std::filesystem::path pathFromUtf8(const std::string& utf8)
{
#if defined(_WIN32)
    return std::filesystem::u8path(utf8);
#else
    return std::filesystem::path(utf8);
#endif
}

bool readWholeFile(const std::string& pathUtf8, std::vector<unsigned char>& out)
{
    const std::filesystem::path p = pathFromUtf8(pathUtf8);
    std::ifstream f(p, std::ios::binary | std::ios::ate);
    if (!f) {
        return false;
    }
    const auto end = f.tellg();
    if (end <= 0) {
        return false;
    }
    f.seekg(0, std::ios::beg);
    out.resize(static_cast<size_t>(end));
    f.read(reinterpret_cast<char*>(out.data()), static_cast<std::streamsize>(out.size()));
    return !f.fail() && f.gcount() == static_cast<std::streamsize>(out.size());
}

} // namespace

static GLuint createGLTexture(const unsigned char* pixels, int w, int h, int ch)
{
    GLenum dataFormat = GL_RGB;
    GLenum internalFormat = GL_RGB8;
    if (ch == 1) {
        dataFormat = GL_RED;
        internalFormat = GL_R8;
    } else if (ch == 2) {
        dataFormat = GL_RG;
        internalFormat = GL_RG8;
    } else if (ch == 3) {
        dataFormat = GL_RGB;
        internalFormat = GL_RGB8;
    } else if (ch == 4) {
        dataFormat = GL_RGBA;
        internalFormat = GL_RGBA8;
    } else {
        std::cerr << "[Tex] unsupported channel count: " << ch << "\n";
        return 0;
    }

    GLuint id = 0;
    glGenTextures(1, &id);
    glBindTexture(GL_TEXTURE_2D, id);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexImage2D(GL_TEXTURE_2D, 0, internalFormat, w, h, 0, dataFormat, GL_UNSIGNED_BYTE, pixels);
    glGenerateMipmap(GL_TEXTURE_2D);
    glBindTexture(GL_TEXTURE_2D, 0);
    return id;
}

GLuint createTexture2DFromRGBAPixels(const void* pixels, int width, int height, bool bgra)
{
    if (!pixels || width <= 0 || height <= 0) {
        std::cerr << "[Tex] invalid raw RGBA texture data.\n";
        return 0;
    }

    GLuint id = 0;
    glGenTextures(1, &id);
    glBindTexture(GL_TEXTURE_2D, id);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexImage2D(
        GL_TEXTURE_2D,
        0,
        GL_RGBA8,
        width,
        height,
        0,
        bgra ? GL_BGRA : GL_RGBA,
        GL_UNSIGNED_BYTE,
        pixels);
    glGenerateMipmap(GL_TEXTURE_2D);
    glBindTexture(GL_TEXTURE_2D, 0);
    return id;
}

GLuint loadTexture2DFromFile(const std::string& path, bool flipV)
{
    if (path.empty()) {
        std::cerr << "[Tex] empty path\n";
        return 0;
    }
    std::vector<unsigned char> bytes;
    if (!readWholeFile(path, bytes)) {
        std::cerr << "[Tex] open failed: " << path << '\n';
        return 0;
    }

    stbi_set_flip_vertically_on_load(flipV ? 1 : 0);
    int w = 0, h = 0, ch = 0;
    unsigned char* data = stbi_load_from_memory(bytes.data(), static_cast<int>(bytes.size()), &w, &h, &ch, 0);
    if (!data) {
        std::cerr << "[Tex] decode failed: " << path << " (" << stbi_failure_reason() << ")\n";
        return 0;
    }
    GLuint id = createGLTexture(data, w, h, ch);
    stbi_image_free(data);
    return id;
}

GLuint loadTexture2DFromMemory(const unsigned char* data, int len, bool flipV)
{
    if (!data || len <= 0) return 0;
    stbi_set_flip_vertically_on_load(flipV ? 1 : 0);

    int w, h, ch;
    unsigned char* pixels = stbi_load_from_memory(data, len, &w, &h, &ch, 0);
    if (!pixels) {
        std::cerr << "[Tex] memory decode failed (" << stbi_failure_reason() << ")\n";
        return 0;
    }
    GLuint id = createGLTexture(pixels, w, h, ch);
    stbi_image_free(pixels);
    return id;
}
