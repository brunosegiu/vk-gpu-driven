set(VERSION 1.0)
set(PROJECT_NAME VK-RT-BAKER)

project(${PROJECT_NAME} VERSION ${VERSION} LANGUAGES CXX)

set(SOURCE
    src/main.cpp
)

set(HEADERS
    include/BakedSceneSerialization.h
)

add_executable(${PROJECT_NAME}
    ${HEADERS}
    ${SOURCE}
)

if(WIN32)
    add_compile_definitions(VKRT_PLATFORM_WINDOWS)
elseif(UNIX)
    add_compile_definitions(VKRT_PLATFORM_LINUX)
endif()
add_compile_definitions(NOMINMAX)

set_target_properties(${PROJECT_NAME} PROPERTIES PUBLIC_HEADER "${HEADERS}")
include_directories(include)

set_property(TARGET ${PROJECT_NAME} PROPERTY CXX_STANDARD 20)
set_property(TARGET ${PROJECT_NAME} PROPERTY CXX_STANDARD_REQUIRED ON)

# Find and link GLM
add_compile_definitions(GLM_FORCE_RADIANS)
add_compile_definitions(GLM_FORCE_DEPTH_ZERO_TO_ONE)
add_compile_definitions(GLM_ENABLE_EXPERIMENTAL)
find_package(glm CONFIG REQUIRED)
target_link_libraries(${PROJECT_NAME} PRIVATE glm::glm)

# Find and link TinyGLTF
add_compile_definitions(TINYGLTF_IMPLEMENTATION)
add_compile_definitions(STB_IMAGE_IMPLEMENTATION)
add_compile_definitions(STB_IMAGE_WRITE_IMPLEMENTATION)
add_compile_definitions(TINYGLTF_NO_INCLUDE_JSON)
find_path(TINYGLTF_INCLUDE_DIRS "tiny_gltf.h")
target_include_directories(${PROJECT_NAME} PRIVATE ${TINYGLTF_INCLUDE_DIRS})

# Find and link JSON (TinyGLTF dependency)
find_package(nlohmann_json CONFIG REQUIRED)
find_path(NLOHMANN_JSON_INCLUDE nlohmann/json.hpp)
include_directories(${NLOHMANN_JSON_INCLUDE})

# Find and link meshoptimizer
find_package(meshoptimizer CONFIG REQUIRED)
target_link_libraries(${PROJECT_NAME} PRIVATE meshoptimizer::meshoptimizer)

if(MSVC)
	set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} /SUBSYSTEM:WINDOWS /ENTRY:mainCRTStartup")
endif()
