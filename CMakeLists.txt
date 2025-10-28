cmake_minimum_required(VERSION 3.10)

if(NOT CMAKE_TOOLCHAIN_FILE)
    set(CMAKE_TOOLCHAIN_FILE ${CMAKE_SOURCE_DIR}/vcpkg/scripts/buildsystems/vcpkg.cmake CACHE STRING "Vcpkg toolchain file")
endif()

project(WormholeSim)

if(MSVC)
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded")
endif()

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(GLEW REQUIRED)
find_package(glfw3 REQUIRED)
find_package(glm REQUIRED)
find_package(OpenGL REQUIRED)

set(DEPS glfw GLEW::GLEW glm::glm OpenGL::GL)

add_executable(WormholeSim wormhole_sim.cpp)
target_link_libraries(WormholeSim PRIVATE ${DEPS})
target_include_directories(WormholeSim PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})

add_custom_command(TARGET WormholeSim POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
            ${CMAKE_CURRENT_SOURCE_DIR}/camera_path.txt
            $<TARGET_FILE_DIR:WormholeSim>
)

find_package(OpenMP)
if(OpenMP_CXX_FOUND)
    target_link_libraries(WormholeSim PRIVATE OpenMP::OpenMP_CXX)
    message(STATUS "OpenMP enabled for WormholeSim - parallel rendering active")
else()
    message(WARNING "OpenMP not found - rendering will be slower")
endif()

add_custom_command(TARGET WormholeSim POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
            ${CMAKE_CURRENT_SOURCE_DIR}/wormhole.comp
            $<TARGET_FILE_DIR:WormholeSim>
)

