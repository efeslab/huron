cmake_minimum_required(VERSION 3.3)  # Least version that understands LLVMCOnfig.cmake

set(CMAKE_CXX_STANDARD 14)
project(huron)

include(CheckCXXCompilerFlag)
CHECK_CXX_COMPILER_FLAG("-std=c++1y" COMPILER_SUPPORTS_CXX1y)
if(COMPILER_SUPPORTS_CXX1y)
	set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++1y -fno-rtti -fPIC -Wall")
else()
	message(STATUS "The compiler ${CMAKE_CXX_COMPILER} has no C++1y support. Please use a different C++ compiler.")
endif()

find_package(LLVM 7 CONFIG)
if (NOT LLVM_FOUND)
    find_package(LLVM 8 REQUIRED CONFIG)
endif ()
list(APPEND CMAKE_MODULE_PATH "${LLVM_CMAKE_DIR}")
include(AddLLVM)
add_definitions(${LLVM_DEFINITIONS})
include_directories(${LLVM_INCLUDE_DIRS})
message(STATUS "Found LLVM ${LLVM_PACKAGE_VERSION}")
message(STATUS "Using LLVMConfig.cmake in: ${LLVM_DIR}")

add_subdirectory(llvm-passes)
add_subdirectory(runtime)
add_subdirectory(postprocess)
add_subdirectory(perf)
