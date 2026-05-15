#include "Shader.h"

#include <glm/gtc/type_ptr.hpp>

#include <fstream>
#include <iostream>
#include <sstream>
#include <utility>

namespace {

bool readShaderFile(const char* path, std::string& output)
{
    if (path == nullptr || path[0] == '\0') {
        std::cerr << "[Shader] File read error: shader path is null or empty.\n";
        return false;
    }

    std::ifstream file(path, std::ios::in);
    if (!file.is_open()) {
        std::cerr << "[Shader] File read error: failed to open shader file.\n"
                  << "  Path: " << path << '\n'
                  << "  Note: relative paths are resolved from the process working directory.\n";
        return false;
    }

    std::ostringstream stream;
    stream << file.rdbuf();

    if (file.bad()) {
        std::cerr << "[Shader] File read error: I/O failure while reading shader file.\n"
                  << "  Path: " << path << '\n';
        return false;
    }

    output = stream.str();
    if (output.empty()) {
        std::cerr << "[Shader] File read warning: shader file is empty.\n"
                  << "  Path: " << path << '\n';
    }

    return true;
}

std::string shaderTypeName(GLenum type)
{
    switch (type) {
    case GL_VERTEX_SHADER:
        return "VERTEX";
    case GL_FRAGMENT_SHADER:
        return "FRAGMENT";
    default:
        return "UNKNOWN";
    }
}

std::string getShaderInfoLog(GLuint shader)
{
    GLint logLength = 0;
    glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &logLength);

    if (logLength <= 1) {
        return {};
    }

    std::string log(static_cast<std::size_t>(logLength), '\0');
    GLsizei written = 0;
    glGetShaderInfoLog(shader, logLength, &written, log.data());
    log.resize(static_cast<std::size_t>(written));
    return log;
}

std::string getProgramInfoLog(GLuint program)
{
    GLint logLength = 0;
    glGetProgramiv(program, GL_INFO_LOG_LENGTH, &logLength);

    if (logLength <= 1) {
        return {};
    }

    std::string log(static_cast<std::size_t>(logLength), '\0');
    GLsizei written = 0;
    glGetProgramInfoLog(program, logLength, &written, log.data());
    log.resize(static_cast<std::size_t>(written));
    return log;
}

void printSourceWithLineNumbers(const std::string& source)
{
    std::istringstream stream(source);
    std::string line;
    int lineNumber = 1;

    while (std::getline(stream, line)) {
        std::cerr << "  " << lineNumber << ": " << line << '\n';
        ++lineNumber;
    }
}

GLuint compileShader(GLenum type, const char* path, const std::string& source)
{
    const GLuint shader = glCreateShader(type);
    if (shader == 0) {
        std::cerr << "[Shader] Compile error: glCreateShader returned 0.\n"
                  << "  Type: " << shaderTypeName(type) << '\n'
                  << "  Path: " << path << '\n';
        return 0;
    }

    const char* sourcePtr = source.c_str();
    glShaderSource(shader, 1, &sourcePtr, nullptr);
    glCompileShader(shader);

    GLint success = GL_FALSE;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
    if (success != GL_TRUE) {
        std::cerr << "[Shader] Compile error.\n"
                  << "  Type: " << shaderTypeName(type) << '\n'
                  << "  Path: " << path << '\n'
                  << "  Source bytes: " << source.size() << '\n'
                  << "  OpenGL info log:\n"
                  << getShaderInfoLog(shader) << '\n'
                  << "  Shader source with line numbers:\n";
        printSourceWithLineNumbers(source);

        glDeleteShader(shader);
        return 0;
    }

    return shader;
}

GLuint linkProgram(GLuint vertexShader, GLuint fragmentShader, const char* vertexPath, const char* fragmentPath)
{
    const GLuint program = glCreateProgram();
    if (program == 0) {
        std::cerr << "[Shader] Link error: glCreateProgram returned 0.\n"
                  << "  Vertex path: " << vertexPath << '\n'
                  << "  Fragment path: " << fragmentPath << '\n';
        return 0;
    }

    glAttachShader(program, vertexShader);
    glAttachShader(program, fragmentShader);
    glLinkProgram(program);

    GLint success = GL_FALSE;
    glGetProgramiv(program, GL_LINK_STATUS, &success);
    if (success != GL_TRUE) {
        std::cerr << "[Shader] Link error.\n"
                  << "  Vertex path: " << vertexPath << '\n'
                  << "  Fragment path: " << fragmentPath << '\n'
                  << "  OpenGL info log:\n"
                  << getProgramInfoLog(program) << '\n';

        glDeleteProgram(program);
        return 0;
    }

    return program;
}

} // namespace

Shader::Shader(const char* vertexPath, const char* fragmentPath)
{
    std::string vertexCode;
    std::string fragmentCode;

    const bool vertexRead = readShaderFile(vertexPath, vertexCode);
    const bool fragmentRead = readShaderFile(fragmentPath, fragmentCode);
    if (!vertexRead || !fragmentRead) {
        std::cerr << "[Shader] Program creation aborted because one or more shader files could not be read.\n"
                  << "  Vertex path: " << (vertexPath ? vertexPath : "<null>") << '\n'
                  << "  Fragment path: " << (fragmentPath ? fragmentPath : "<null>") << '\n';
        return;
    }

    const GLuint vertexShader = compileShader(GL_VERTEX_SHADER, vertexPath, vertexCode);
    const GLuint fragmentShader = compileShader(GL_FRAGMENT_SHADER, fragmentPath, fragmentCode);

    if (vertexShader != 0 && fragmentShader != 0) {
        ID = linkProgram(vertexShader, fragmentShader, vertexPath, fragmentPath);
    } else {
        std::cerr << "[Shader] Program creation aborted because shader compilation failed.\n"
                  << "  Vertex shader object: " << vertexShader << '\n'
                  << "  Fragment shader object: " << fragmentShader << '\n';
    }

    if (vertexShader != 0) {
        glDeleteShader(vertexShader);
    }
    if (fragmentShader != 0) {
        glDeleteShader(fragmentShader);
    }
}

Shader::~Shader()
{
    if (ID != 0) {
        glDeleteProgram(ID);
    }
}

Shader::Shader(Shader&& other) noexcept
    : ID(std::exchange(other.ID, 0))
{
}

Shader& Shader::operator=(Shader&& other) noexcept
{
    if (this != &other) {
        if (ID != 0) {
            glDeleteProgram(ID);
        }
        ID = std::exchange(other.ID, 0);
    }

    return *this;
}

void Shader::use() const
{
    if (ID == 0) {
        std::cerr << "[Shader] use() ignored: shader program is invalid.\n";
        return;
    }

    glUseProgram(ID);
}

void Shader::setBool(const std::string& name, bool value) const
{
    glUniform1i(getUniformLocation(name), value ? 1 : 0);
}

void Shader::setInt(const std::string& name, int value) const
{
    glUniform1i(getUniformLocation(name), value);
}

void Shader::setFloat(const std::string& name, float value) const
{
    glUniform1f(getUniformLocation(name), value);
}

void Shader::setVec3(const std::string& name, const glm::vec3& value) const
{
    glUniform3fv(getUniformLocation(name), 1, glm::value_ptr(value));
}

void Shader::setVec2(const std::string& name, const glm::vec2& value) const
{
    glUniform2fv(getUniformLocation(name), 1, glm::value_ptr(value));
}

void Shader::setMat4(const std::string& name, const glm::mat4& mat) const
{
    glUniformMatrix4fv(getUniformLocation(name), 1, GL_FALSE, glm::value_ptr(mat));
}

GLint Shader::getUniformLocation(const std::string& name) const
{
    if (ID == 0) {
        std::cerr << "[Shader] Uniform lookup failed: shader program is invalid.\n"
                  << "  Uniform name: " << name << '\n';
        return -1;
    }

    const GLint location = glGetUniformLocation(ID, name.c_str());
    if (location == -1) {
        std::cerr << "[Shader] Uniform warning: uniform was not found or was optimized out.\n"
                  << "  Program ID: " << ID << '\n'
                  << "  Uniform name: " << name << '\n';
    }

    return location;
}
