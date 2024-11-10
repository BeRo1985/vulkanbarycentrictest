cmake_minimum_required(VERSION 3.10)
project(vulkanbarycentrictest)

set(CMAKE_C_STANDARD 11)

# Find Vulkan
find_package(Vulkan REQUIRED)

# Find SDL2
find_package(SDL2 REQUIRED)
include_directories(${SDL2_INCLUDE_DIRS})

# Shader compilation
add_custom_command(
  OUTPUT ${CMAKE_BINARY_DIR}/triangle.vert.spv
  COMMAND glslangValidator -V ${CMAKE_SOURCE_DIR}/triangle.vert.glsl -S vert --target-env vulkan1.3 -o ${CMAKE_BINARY_DIR}/triangle.vert.spv
  DEPENDS ${CMAKE_SOURCE_DIR}/triangle.vert.glsl
)

add_custom_command(
  OUTPUT ${CMAKE_BINARY_DIR}/triangle.frag.spv
  COMMAND glslangValidator -V ${CMAKE_SOURCE_DIR}/triangle.frag.glsl -S frag --target-env vulkan1.3 -o ${CMAKE_BINARY_DIR}/triangle.frag.spv
  DEPENDS ${CMAKE_SOURCE_DIR}/triangle.frag.glsl
)

add_custom_target(
  Shaders
  DEPENDS ${CMAKE_BINARY_DIR}/triangle.vert.spv ${CMAKE_BINARY_DIR}/triangle.frag.spv
)

# Add executable
add_executable(vulkanbarycentrictest vulkanbarycentrictest.c)

# Make sure shaders are built before the executable
add_dependencies(vulkanbarycentrictest Shaders)

# Link libraries
target_link_libraries(vulkanbarycentrictest ${Vulkan_LIBRARIES} ${SDL2_LIBRARIES} m)
