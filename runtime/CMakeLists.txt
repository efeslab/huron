set(SOURCE_FILES LoggingThread.cpp Runtime.cpp LoggingThread.h GetGlobal.h xthread.h MemArith.h MallocInfo.h Segment.h
        LibFuncs.h SymbolCache.h SharedSpinLock.h)
add_library(runtime SHARED ${SOURCE_FILES})
target_link_libraries(runtime dl pthread)
set(CMAKE_CXX_FLAGS "-Wall -Wextra -Wno-unused-private-field -DDEBUG -fPIC")
set(CMAKE_CXX_FLAGS_DEBUG "-g -Og")
set(CMAKE_CXX_FLAGS_RELEASE "-O3 -g")
