cmake_minimum_required(VERSION 3.21)
project(WormholeSim)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Dependencies
find_package(GLEW REQUIRED)
find_package(glfw3 CONFIG REQUIRED)
find_package(glm CONFIG REQUIRED)
find_package(OpenGL REQUIRED)

# Common dependencies
set(DEPS glfw GLEW::GLEW glm::glm OpenGL::GL)

# Wormhole Simulation executable (new)
add_executable(WormholeSim wormhole_sim.cpp)
target_link_libraries(WormholeSim PRIVATE ${DEPS})
target_include_directories(WormholeSim PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
add_custom_command(TARGET WormholeSim POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
            ${CMAKE_CURRENT_SOURCE_DIR}/camera_path.txt
            $<TARGET_FILE_DIR:WormholeSim>
)

# Enable OpenMP for parallel rendering
find_package(OpenMP)
if(OpenMP_CXX_FOUND)
    target_link_libraries(WormholeSim PRIVATE OpenMP::OpenMP_CXX)
    message(STATUS "OpenMP enabled for WormholeSim - parallel rendering active")
else()
    message(WARNING "OpenMP not found - rendering will be slower")
endif()

# Shader files (copy to output dir)
file(GLOB SHADERS
    "${CMAKE_CURRENT_SOURCE_DIR}/*.vert"
    "${CMAKE_CURRENT_SOURCE_DIR}/*.frag"
    "${CMAKE_CURRENT_SOURCE_DIR}/*.comp"
)

foreach(shader ${SHADERS})
    add_custom_command(TARGET WormholeSim POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
        ${shader}
        $<TARGET_FILE_DIR:WormholeSim>
    )
endforeach()

