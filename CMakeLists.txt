cmake_minimum_required(VERSION 3.16)

project(ClassroomRenderer LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(MSVC)
    add_compile_options("/FS" "/Z7")
    set(CMAKE_MSVC_DEBUG_INFORMATION_FORMAT "Embedded")
endif()

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/ThirdParty")
    set(THIRD_PARTY_DIR "${CMAKE_CURRENT_SOURCE_DIR}/ThirdParty")
else()
    message(FATAL_ERROR "Third-party directory not found. Expected ThirdParty.")
endif()

set(GLAD_ROOT "${THIRD_PARTY_DIR}/glad")
set(STB_ROOT "${THIRD_PARTY_DIR}/stb")

if(NOT EXISTS "${GLAD_ROOT}/include/glad/glad.h")
    message(FATAL_ERROR "GLAD header not found at ${GLAD_ROOT}/include/glad/glad.h")
endif()

if(NOT EXISTS "${STB_ROOT}/stb_image.h")
    message(FATAL_ERROR "stb_image.h not found at ${STB_ROOT}/stb_image.h")
endif()

include(FetchContent)

add_library(glad STATIC
    "${GLAD_ROOT}/src/glad.c"
)

target_include_directories(glad PUBLIC
    "${GLAD_ROOT}/include"
)

set(GLFW_BUILD_DOCS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(GLFW_INSTALL OFF CACHE BOOL "" FORCE)
set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)

if(EXISTS "${THIRD_PARTY_DIR}/glfw/CMakeLists.txt")
    add_subdirectory("${THIRD_PARTY_DIR}/glfw")
else()
    FetchContent_Declare(
        glfw
        GIT_REPOSITORY https://github.com/glfw/glfw.git
        GIT_TAG 3.4
    )
    FetchContent_MakeAvailable(glfw)
endif()

set(ASSIMP_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(ASSIMP_BUILD_ASSIMP_TOOLS OFF CACHE BOOL "" FORCE)
set(ASSIMP_BUILD_SAMPLES OFF CACHE BOOL "" FORCE)
set(ASSIMP_INSTALL OFF CACHE BOOL "" FORCE)
set(ASSIMP_WARNINGS_AS_ERRORS OFF CACHE BOOL "" FORCE)
set(ASSIMP_NO_EXPORT ON CACHE BOOL "" FORCE)

if(EXISTS "${THIRD_PARTY_DIR}/assimp/CMakeLists.txt")
    add_subdirectory("${THIRD_PARTY_DIR}/assimp")
else()
    FetchContent_Declare(
        assimp
        GIT_REPOSITORY https://github.com/assimp/assimp.git
        GIT_TAG v5.4.3
    )
    FetchContent_MakeAvailable(assimp)
endif()

add_executable(ClassroomRenderer
    src/main.cpp
    src/core/Application.cpp
    src/core/Application.h
    src/core/Camera.h
    src/core/Transform.h
    src/render/Renderer.cpp
    src/render/Renderer.h
    src/render/ShadowMap.cpp
    src/render/ShadowMap.h
    src/render/Material.h
    src/render/Shader.cpp
    src/render/Shader.h
    src/render/Mesh.cpp
    src/render/Mesh.h
    src/render/Texture.cpp
    src/render/Texture.h
    src/scene/Scene.cpp
    src/scene/Scene.h
    src/scene/Model.cpp
    src/scene/Model.h
)

target_include_directories(ClassroomRenderer PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
    "${THIRD_PARTY_DIR}/glm"
    "${STB_ROOT}"
)

if(MSVC)
    target_compile_options(ClassroomRenderer PRIVATE "/utf-8")
endif()

target_link_libraries(ClassroomRenderer PRIVATE
    glad
    glfw
    assimp
)
