cmake_minimum_required(VERSION 3.16 FATAL_ERROR)

# Folder structure
set_property(GLOBAL PROPERTY USE_FOLDERS ON)
set(CMAKE_SKIP_INSTALL_ALL_DEPENDENCY ON)
set(CMAKE_FOLDER CMakePredefinedTargets)

# Project
project(VkLectureSamples LANGUAGES CXX)
set(CMAKE_CONFIGURATION_TYPES Debug Release)

# Add source
add_executable(VkLectureSamples src/main.cpp)
target_compile_features(VkLectureSamples PRIVATE cxx_std_17)
set_property(TARGET VkLectureSamples PROPERTY FOLDER "")

# Add GLFW
add_subdirectory(deps/glfw)
target_link_libraries(VkLectureSamples glfw)

# Add Vulkan
find_package(Vulkan REQUIRED)
include_directories(${Vulkan_INCLUDE_DIRS})
target_link_libraries(VkLectureSamples ${Vulkan_LIBRARIES})
