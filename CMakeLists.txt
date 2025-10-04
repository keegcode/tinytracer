cmake_minimum_required (VERSION 3.8)

set(DCMAKE_TOOLCHAIN_FILE, "${VCPKG_ROOT}/scripts/buildsystems/vcpkg.cmake")

project ("raytracer")

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED True)
set(CMAKE_EXPORT_COMPILE_COMMANDS True)

add_executable(RayTracer "${CMAKE_CURRENT_SOURCE_DIR}/src/main.cpp")

file(GLOB_RECURSE RayTracer_Sources
        "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/src/*.h"
        "${CMAKE_CURRENT_SOURCE_DIR}/src/*.hpp"
)
    
find_package(glm CONFIG REQUIRED)
find_package(SDL3 CONFIG REQUIRED)
find_package(fmt CONFIG REQUIRED)

target_sources(RayTracer PRIVATE ${RayTracer_Sources})
target_include_directories(RayTracer PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_link_libraries(RayTracer PRIVATE glm::glm SDL3::SDL3 fmt::fmt)

if (MSVC)
        target_compile_options(RayTracer PRIVATE
                /W3
                /MP
                /ZI
        )
else()
        target_compile_options(RayTracer PRIVATE
                -Wall
                -Wpedantic
        )
endif()