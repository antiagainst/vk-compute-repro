cmake_minimum_required(VERSION 3.7)
project(VkComputeRepro)
set(CMAKE_CXX_STANDARD 11)
find_package(Vulkan)
add_executable(repro repro.cpp)
target_include_directories(repro PRIVATE ${Vulkan_INCLUDE_DIR})
target_link_libraries(repro ${Vulkan_LIBRARY})
