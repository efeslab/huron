SOURCE_DIR = .
INCLUDE_DIR = .

SRCS =  $(SOURCE_DIR)/Runtime.cpp   \
		$(SOURCE_DIR)/LoggingThread.cpp

INCS =  $(INCLUDE_DIR)/GetGlobal.h        \
        $(INCLUDE_DIR)/LoggingThread.h    \
        $(INCLUDE_DIR)/MallocInfo.h       \
        $(INCLUDE_DIR)/MemArith.h 		  \
		$(INCLUDE_DIR)/xthread.h          \
		$(INCLUDE_DIR)/LibFuncs.h         \

DEPS = $(SRCS) $(INCS)

CXX = clang++ -std=c++1z -g -O3 -I$(INCLUDE_DIR)

CFLAGS   = -fno-omit-frame-pointer -DDEBUG
CFLAGS32 = $(CFLAGS) -m32 -DX86_32BIT
CFLAGS64 = $(CFLAGS)

TARGETS = libruntime.so

all: $(TARGETS)

libruntime.so: $(DEPS)
	$(CXX) $(CFLAGS64) $(INCLUDE_DIRS) -shared -fPIC $(SRCS) -o libruntime.so -ldl -lpthread

clean:
	rm -f $(TARGETS)
