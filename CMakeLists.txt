cmake_minimum_required(VERSION 3.16)

project(ClassroomRenderer LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/ThirdParty")
    set(THIRD_PARTY_DIR "${CMAKE_CURRENT_SOURCE_DIR}/ThirdParty")
elseif(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/Thirdparty")
    set(THIRD_PARTY_DIR "${CMAKE_CURRENT_SOURCE_DIR}/Thirdparty")
else()
    message(FATAL_ERROR "Third-party directory not found. Expected ThirdParty or Thirdparty.")
endif()

set(GLAD_ROOT "${THIRD_PARTY_DIR}/glad")

if(NOT EXISTS "${GLAD_ROOT}/include/glad/glad.h")
    message(FATAL_ERROR "GLAD header not found at ${GLAD_ROOT}/include/glad/glad.h")
endif()

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
    include(FetchContent)
    FetchContent_Declare(
        glfw
        GIT_REPOSITORY https://github.com/glfw/glfw.git
        GIT_TAG 3.4
    )
    FetchContent_MakeAvailable(glfw)
endif()

add_executable(ClassroomRenderer
    src/main.cpp
    src/Camera.h
    src/Shader.cpp
    src/Shader.h
)

target_include_directories(ClassroomRenderer PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
    "${THIRD_PARTY_DIR}/glm"
)

target_link_libraries(ClassroomRenderer PRIVATE
    glad
    glfw
)
