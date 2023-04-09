cmake_minimum_required (VERSION 3.8)
set(VCPKG_TARGET_TRIPLET x64-windows)

project ("TuranRT")
set(CMAKE_C_STANDARD 17)
set(CMAKE_CXX_STANDARD 17)
add_subdirectory("src")
add_subdirectory("dependencies/TuranLibraries")