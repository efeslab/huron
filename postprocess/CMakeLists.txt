set(CMAKE_CXX_FLAGS "-Wall -Wextra")
set(CMAKE_CXX_FLAGS_DEBUG "-g")
set(CMAKE_CXX_FLAGS_RELEASE "-O3 -g")

add_executable(postprocess main.cpp
        Utils.h Utils.cpp
        Repair.h
        Stats.h
        Detect.h Detect.cpp Repair.cpp)
