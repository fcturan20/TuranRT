file(GLOB_RECURSE EDITORFILES "editor/*.cpp" "editor/*.h" "editor/*.c")
add_executable(RTEditor ${EDITORFILES} ${VKFILES} ${TAPIFILES})
set_target_properties(RTEditor PROPERTIES RUNTIME_OUTPUT_DIRECTORY_DEBUG ${CMAKE_BINARY_DIR}/Debug ARCHIVE_OUTPUT_DIRECTORY_DEBUG ${CMAKE_BINARY_DIR}/Debug)
set_target_properties(RTEditor PROPERTIES RUNTIME_OUTPUT_DIRECTORY_RELEASE ${CMAKE_BINARY_DIR}/Release ARCHIVE_OUTPUT_DIRECTORY_RELEASE ${CMAKE_BINARY_DIR}/Release PREFIX "")
set_target_properties(RTEditor PROPERTIES RUNTIME_OUTPUT_DIRECTORY_RELWITHDEBINFO ${CMAKE_BINARY_DIR}/ReleaseDbg RUNTIME_OUTPUT_DIRECTORY_RELWITHDEBINFO ${CMAKE_BINARY_DIR}/ReleaseDbg PREFIX "")
target_compile_definitions(RTEditor PUBLIC SOURCE_DIR="${CMAKE_CURRENT_SOURCE_DIR}/../")
set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -DVULKAN_DEBUGGING")

find_package(glm REQUIRED)
target_include_directories(RTEditor PRIVATE ${glm_INCLUDE_DIRS})
target_link_libraries(RTEditor PRIVATE glm::glm)

find_package(Flatbuffers CONFIG REQUIRED)
    target_link_libraries(RTEditor PRIVATE flatbuffers::flatbuffers)

    
find_package(assimp CONFIG REQUIRED)
target_link_libraries(RTEditor PRIVATE assimp::assimp)


find_package(glfw3 CONFIG REQUIRED)
if(GLFW_FOUND OR glfw3_FOUND)
    target_include_directories(RTEditor PUBLIC ${GLFW_INCLUDE_DIRS})
    target_link_libraries(RTEditor PUBLIC glfw)
else()
    message ("GLFW isn't found, please install glfw with vcpkg")
endif()


find_package(glm REQUIRED)
target_include_directories(RTEditor PUBLIC ${glm_INCLUDE_DIRS})
target_link_libraries(RTEditor PUBLIC glm::glm)


find_package(Vulkan REQUIRED)
if(VULKAN_FOUND)
    target_include_directories(RTEditor PUBLIC ${VULKAN_SDK}/Include)
    message(${VULKAN_SDK}/Include)
    target_link_libraries(RTEditor PUBLIC Vulkan::Vulkan)
else()
    message("Vulkan isn't found, please be sure to install Vulkan SDK then include and link Vulkan to the project!")
endif()


find_package(imgui CONFIG REQUIRED)
if(imgui_FOUND)
    target_link_libraries(RTEditor PRIVATE imgui::imgui)
else()
    message("dear Imgui (by Ocornut) isn't installed on your system!")
endif()

#To compile glsl shaders to SPIR-V in run-time
find_package(glslang CONFIG REQUIRED)
target_link_libraries(RTEditor PRIVATE glslang::OSDependent glslang::glslang glslang::MachineIndependent glslang::GenericCodeGen glslang::glslang-default-resource-limits glslang::OGLCompiler glslang::SPVRemapper glslang::SPIRV glslang::HLSL)


target_include_directories(RTEditor PUBLIC ${CMAKE_SOURCE_DIR}/dependencies)
target_link_libraries(RTEditor PRIVATE tgfx_core TGFXVulkan TGFXVulkanGlslang tapi_ecs tapi_string tapi_bitset tapi_filesys tapi_logger tapi_profiler tapi_threadedjobsys tapi_unittest)