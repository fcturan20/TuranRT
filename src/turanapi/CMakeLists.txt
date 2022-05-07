file(GLOB_RECURSE TAPIFILES "*.c" "*.cpp" "*.h")
add_library(tapi_systems ${TAPIFILES})
