set(SOURCE_FILES Perf.h Perf.cpp Runtime.cpp ProcMaps.cpp ProcMaps.h
        LoggingThread.h LoggingThread.cpp
        Segment.h xthread.h)
add_library(perf SHARED ${SOURCE_FILES})
target_link_libraries(perf dl pthread)
set(CMAKE_CXX_FLAGS "-Wall -Wextra -Wno-unused-parameter -fPIC")
set(CMAKE_CXX_FLAGS_DEBUG "-g")
set(CMAKE_CXX_FLAGS_RELEASE "-O3 -g")
