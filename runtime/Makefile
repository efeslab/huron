SOURCE_DIR = .
INCLUDE_DIR = .

SRCS =  $(SOURCE_DIR)/Runtime.cpp   \
		$(SOURCE_DIR)/LoggingThread.cpp

INCS =  $(INCLUDE_DIR)/GetGlobal.h       \
        $(INCLUDE_DIR)/LoggingThread.h   \
        $(INCLUDE_DIR)/MallocInfo.h       \
        $(INCLUDE_DIR)/MemArith.h 		 \
		$(INCLUDE_DIR)/xthread.h         \

DEPS = $(SRCS) $(INCS)

CXX = g++ -std=c++11 -g -O3 -I$(INCLUDE_DIR)

CFLAGS   = -fno-omit-frame-pointer -DDEBUG
CFLAGS32 = $(CFLAGS) -m32 -DX86_32BIT # -O3
CFLAGS64 = $(CFLAGS) #-m64 # -O3

TARGETS = libruntime.so

all: $(TARGETS)

libruntime.so: $(DEPS)
	$(CXX) $(CFLAGS64) $(INCLUDE_DIRS) -shared -fPIC $(SRCS) -o libruntime.so -ldl -lpthread

clean:
	rm -f $(TARGETS)
