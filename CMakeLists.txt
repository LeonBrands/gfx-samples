cmake_minimum_required(VERSION 3.0 FATAL_ERROR)

# Folder structure
set_property(GLOBAL PROPERTY USE_FOLDERS ON)
set(CMAKE_SKIP_INSTALL_ALL_DEPENDENCY ON)
set(CMAKE_FOLDER CMakePredefinedTargets)

# Project
project(gfx-samples LANGUAGES CXX)
set(CMAKE_CONFIGURATION_TYPES Debug Release)

add_subdirectory(deps/glfw)
set_property(TARGET glfw PROPERTY FOLDER "deps/glfw3")
set_property(TARGET uninstall PROPERTY FOLDER "deps/glfw3")
set_property(TARGET update_mappings PROPERTY FOLDER "deps/glfw3")

add_subdirectory(vk)