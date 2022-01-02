add_executable(000_clear 000_clear/main.cpp)
target_compile_features(000_clear PRIVATE cxx_std_17)
set_property(TARGET 000_clear PROPERTY FOLDER "gfx-samples/vk")

add_executable(001_triangle 001_triangle/main.cpp)
target_compile_features(001_triangle PRIVATE cxx_std_17)
set_property(TARGET 001_triangle PROPERTY FOLDER "gfx-samples/vk")

add_executable(002_vertex_buffer 002_vertex_buffer/main.cpp)
target_compile_features(002_vertex_buffer PRIVATE cxx_std_17)
set_property(TARGET 002_vertex_buffer PROPERTY FOLDER "gfx-samples/vk")

add_executable(003_restructure 
    003_restructure/main.cpp 
    003_restructure/utils/memory.hpp
    003_restructure/utils/queue_families.hpp
    003_restructure/utils/buffer.hpp
    003_restructure/utils/layers.hpp
    003_restructure/utils/physical_device.hpp
    003_restructure/utils/swapchain.hpp
    003_restructure/utils/shader.hpp
    003_restructure/utils/preprocessor.hpp
    003_restructure/utils/extensions.hpp)
target_compile_features(003_restructure PRIVATE cxx_std_17)
set_property(TARGET 003_restructure PROPERTY FOLDER "gfx-samples/vk")

add_executable(004_index_buffer
    004_index_buffer/main.cpp 
    004_index_buffer/utils/memory.hpp
    004_index_buffer/utils/queue_families.hpp
    004_index_buffer/utils/buffer.hpp
    004_index_buffer/utils/layers.hpp
    004_index_buffer/utils/physical_device.hpp
    004_index_buffer/utils/swapchain.hpp
    004_index_buffer/utils/shader.hpp
    004_index_buffer/utils/preprocessor.hpp
    004_index_buffer/utils/extensions.hpp)
target_compile_features(004_index_buffer PRIVATE cxx_std_17)
set_property(TARGET 004_index_buffer PROPERTY FOLDER "gfx-samples/vk")

add_executable(005_push_constants
    005_push_constants/main.cpp 
    005_push_constants/utils/memory.hpp
    005_push_constants/utils/queue_families.hpp
    005_push_constants/utils/buffer.hpp
    005_push_constants/utils/layers.hpp
    005_push_constants/utils/physical_device.hpp
    005_push_constants/utils/swapchain.hpp
    005_push_constants/utils/shader.hpp
    005_push_constants/utils/preprocessor.hpp
    005_push_constants/utils/extensions.hpp)
target_compile_features(005_push_constants PRIVATE cxx_std_17)
set_property(TARGET 005_push_constants PROPERTY FOLDER "gfx-samples/vk")

target_link_libraries(000_clear glfw)
target_link_libraries(001_triangle glfw)
target_link_libraries(002_vertex_buffer glfw)
target_link_libraries(003_restructure glfw)
target_link_libraries(004_index_buffer glfw)
target_link_libraries(005_push_constants glfw)

# Add Vulkan
find_package(Vulkan REQUIRED)
include_directories(${Vulkan_INCLUDE_DIRS})
target_link_libraries(000_clear ${Vulkan_LIBRARIES})
target_link_libraries(001_triangle ${Vulkan_LIBRARIES})
target_link_libraries(002_vertex_buffer ${Vulkan_LIBRARIES})
target_link_libraries(003_restructure ${Vulkan_LIBRARIES})
target_link_libraries(004_index_buffer ${Vulkan_LIBRARIES})
target_link_libraries(005_push_constants ${Vulkan_LIBRARIES})
