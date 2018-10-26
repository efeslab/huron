cmake_minimum_required(VERSION 3.9.6)
set(CMAKE_CXX_STANDARD 17)
project(huron)

find_package(LLVM REQUIRED CONFIG)
list(APPEND CMAKE_MODULE_PATH "${LLVM_CMAKE_DIR}")
include(AddLLVM)

add_definitions(${LLVM_DEFINITIONS})
include_directories(${LLVM_INCLUDE_DIRS})

add_subdirectory(Instrumenter)
add_subdirectory(RedirectPtr)
add_subdirectory(postprocess)
add_subdirectory(runtime)
