file(GLOB_RECURSE VKFILES "vulkan_backend/*.cpp" "vulkan_backend/*.h" "vulkan_backend/*.c")
file(GLOB_RECURSE EDITORFILES "editor/*.cpp" "editor/*.h" "editor/*.c" "filesys/*.cpp" "filesys/*.h" "filesys/*.c")
file(GLOB_RECURSE TAPIFILES "turanapi/*.c" "turanapi/*.cpp" "turanapi/*.h")
add_executable(editor ${EDITORFILES} ${VKFILES} ${TAPIFILES})
set_target_properties(editor PROPERTIES RUNTIME_OUTPUT_DIRECTORY_DEBUG ${CMAKE_BINARY_DIR}/Debug ARCHIVE_OUTPUT_DIRECTORY_DEBUG ${CMAKE_BINARY_DIR}/Debug)
set_target_properties(editor PROPERTIES RUNTIME_OUTPUT_DIRECTORY_RELEASE ${CMAKE_BINARY_DIR}/Release ARCHIVE_OUTPUT_DIRECTORY_RELEASE ${CMAKE_BINARY_DIR}/Release PREFIX "")
set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -DVULKAN_DEBUGGING")

find_package(glm REQUIRED)
target_include_directories(editor PRIVATE ${glm_INCLUDE_DIRS})
target_link_libraries(editor PRIVATE glm::glm)

find_package(Flatbuffers CONFIG REQUIRED)
    target_link_libraries(editor PRIVATE flatbuffers::flatbuffers)

    
find_package(assimp CONFIG REQUIRED)
target_link_libraries(editor PRIVATE assimp::assimp)


find_package(glfw3 CONFIG REQUIRED)
if(GLFW_FOUND OR glfw3_FOUND)
    target_include_directories(editor PUBLIC ${GLFW_INCLUDE_DIRS})
    target_link_libraries(editor PUBLIC glfw)
else()
    message ("GLFW isn't found, please install glfw with vcpkg")
endif()


find_package(glm REQUIRED)
target_include_directories(editor PUBLIC ${glm_INCLUDE_DIRS})
target_link_libraries(editor PUBLIC glm::glm)


find_package(Vulkan REQUIRED)
if(VULKAN_FOUND)
    target_include_directories(editor PUBLIC ${VULKAN_SDK}/Include)
    message(${VULKAN_SDK}/Include)
    target_link_libraries(editor PUBLIC Vulkan::Vulkan)
else()
    message("Vulkan isn't found, please be sure to install Vulkan SDK then include and link Vulkan to the project!")
endif()


find_package(imgui CONFIG REQUIRED)
if(imgui_FOUND)
    target_link_libraries(editor PRIVATE imgui::imgui)
else()
    message("dear Imgui (by Ocornut) isn't installed on your system!")
endif()

#To compile glsl shaders to SPIR-V in run-time
find_package(glslang CONFIG REQUIRED)
target_link_libraries(editor PRIVATE HLSL SPIRV glslang OGLCompiler)


target_include_directories(editor PUBLIC ${CMAKE_SOURCE_DIR}/src)