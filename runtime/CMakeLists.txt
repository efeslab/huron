cmake_minimum_required(VERSION 3.9)
set(CMAKE_CXX_STANDARD 11)

project(runtime VERSION 0.1.0 DESCRIPTION "runtime dyn library for profiling")
set(SOURCE_FILES LoggingThread.cpp Runtime.cpp)
set(HEADER_FILES LoggingThread.h GetGlobal.h xthread.h MemArith.h)
add_library(runtime SHARED ${SOURCE_FILES} ${HEADER_FILES})
target_link_libraries(runtime dl pthread)
target_compile_options(runtime PRIVATE -g -O2 -fno-omit-frame-pointer -DDEBUG -fPIC)
