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

add_executable(002_vertex_buffer src/002_vertex_buffer/main.cpp)
target_compile_features(002_vertex_buffer PRIVATE cxx_std_17)
set_property(TARGET 002_vertex_buffer PROPERTY FOLDER "")

add_executable(003_restructure 
    src/003_restructure/main.cpp 
    src/003_restructure/utils/memory.hpp
    src/003_restructure/utils/queue_families.hpp
    src/003_restructure/utils/buffer.hpp
    src/003_restructure/utils/layers.hpp
    src/003_restructure/utils/physical_device.hpp
    src/003_restructure/utils/swapchain.hpp
    src/003_restructure/utils/shader.hpp
    src/003_restructure/utils/preprocessor.hpp
    src/003_restructure/utils/extensions.hpp)
target_compile_features(003_restructure PRIVATE cxx_std_17)
set_property(TARGET 003_restructure PROPERTY FOLDER "")

add_executable(004_index_buffer
    src/004_index_buffer/main.cpp 
    src/004_index_buffer/utils/memory.hpp
    src/004_index_buffer/utils/queue_families.hpp
    src/004_index_buffer/utils/buffer.hpp
    src/004_index_buffer/utils/layers.hpp
    src/004_index_buffer/utils/physical_device.hpp
    src/004_index_buffer/utils/swapchain.hpp
    src/004_index_buffer/utils/shader.hpp
    src/004_index_buffer/utils/preprocessor.hpp
    src/004_index_buffer/utils/extensions.hpp)
target_compile_features(004_index_buffer PRIVATE cxx_std_17)
set_property(TARGET 004_index_buffer PROPERTY FOLDER "")

# Add GLFW
add_subdirectory(deps/glfw)
target_link_libraries(000_clear glfw)
target_link_libraries(001_triangle glfw)
target_link_libraries(002_vertex_buffer glfw)
target_link_libraries(003_restructure glfw)
target_link_libraries(004_index_buffer glfw)

# Add Vulkan
find_package(Vulkan REQUIRED)
include_directories(${Vulkan_INCLUDE_DIRS})
target_link_libraries(000_clear ${Vulkan_LIBRARIES})
target_link_libraries(001_triangle ${Vulkan_LIBRARIES})
target_link_libraries(002_vertex_buffer ${Vulkan_LIBRARIES})
target_link_libraries(003_restructure ${Vulkan_LIBRARIES})
target_link_libraries(004_index_buffer ${Vulkan_LIBRARIES})
