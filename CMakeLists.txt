cmake_minimum_required(VERSION 3.16)

project(OpenGLDemo LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)

set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_CXX_EXTENSIONS OFF)

option(BUILD_DEMOS "Build demonstration applications" ON)
include(CTest)

add_library(stb_image STATIC EXCLUDE_FROM_ALL third_party/stb/stb_image_impl.cpp third_party/stb/decode_memory.cpp)
add_library(miniaudio STATIC EXCLUDE_FROM_ALL third_party/miniaudio/miniaudio_impl.cpp)

target_include_directories(stb_image SYSTEM PRIVATE third_party/stb)
target_include_directories(miniaudio SYSTEM PRIVATE third_party/miniaudio)

if(UNIX AND NOT APPLE)
    set(THREADS_PREFER_PTHREAD_FLAG TRUE)
    find_package(Threads REQUIRED)
    target_link_libraries(miniaudio PRIVATE Threads::Threads ${CMAKE_DL_LIBS} m)
endif()

add_library(project_warnings INTERFACE)
if(MSVC)
    target_compile_options(project_warnings INTERFACE /W4)
else()
    target_compile_options(project_warnings INTERFACE
        -Wall -Wextra -Wpedantic -Wconversion -Wsign-conversion)
endif()

# Движок собирается всегда и не зависит от демо и тестов.
add_subdirectory(engine)
add_subdirectory(platform)

if(BUILD_DEMOS)
    add_subdirectory(demos)
endif()

if(BUILD_TESTING)
    add_subdirectory(tests)
endif()
