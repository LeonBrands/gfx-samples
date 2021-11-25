cmake_minimum_required(VERSION 3.0 FATAL_ERROR)

# Folder structure
set_property(GLOBAL PROPERTY USE_FOLDERS ON)
set(CMAKE_SKIP_INSTALL_ALL_DEPENDENCY ON)
set(CMAKE_FOLDER CMakePredefinedTargets)

# Project
project(VkSamples LANGUAGES CXX)
set(CMAKE_CONFIGURATION_TYPES Debug Release)

# Add source
add_executable(000_clear src/000_clear/main.cpp)
target_compile_features(000_clear PRIVATE cxx_std_17)
set_property(TARGET 000_clear PROPERTY FOLDER "")

add_executable(001_triangle src/001_triangle/main.cpp)
target_compile_features(001_triangle PRIVATE cxx_std_17)
set_property(TARGET 001_triangle PROPERTY FOLDER "")

# Add GLFW
add_subdirectory(deps/glfw)
target_link_libraries(000_clear glfw)
target_link_libraries(001_triangle glfw)

# Add Vulkan
find_package(Vulkan REQUIRED)
include_directories(${Vulkan_INCLUDE_DIRS})
target_link_libraries(000_clear ${Vulkan_LIBRARIES})
target_link_libraries(001_triangle ${Vulkan_LIBRARIES})
