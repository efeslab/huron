cmake_minimum_required(VERSION 3.3)  # Least version that understands LLVMCOnfig.cmake

set(CMAKE_CXX_STANDARD 17)
project(huron)

find_package(LLVM 7 CONFIG)
#if (NOT LLVM)
#    find_package(LLVM 8 REQUIRED CONFIG)
#endif ()
list(APPEND CMAKE_MODULE_PATH "${LLVM_CMAKE_DIR}")
include(AddLLVM)
add_definitions(${LLVM_DEFINITIONS})
include_directories(${LLVM_INCLUDE_DIRS})
message(STATUS "Found LLVM ${LLVM_PACKAGE_VERSION}")
message(STATUS "Using LLVMConfig.cmake in: ${LLVM_DIR}")

add_subdirectory(llvm-passes)
add_subdirectory(runtime)
add_subdirectory(postprocess)
