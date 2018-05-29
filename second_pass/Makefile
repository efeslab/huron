SOURCE_DIR = .
INCLUDE_DIR = .

SRCS =  $(SOURCE_DIR)/SecondPass.cpp

INCS =  

DEPS = $(SRCS) $(INCS)

CXX = g++ -std=c++11 -g -O2 -I$(INCLUDE_DIR)

CFLAGS   = -fno-omit-frame-pointer -DDEBUG
CFLAGS32 = $(CFLAGS) -m32 -DX86_32BIT # -O3
CFLAGS64 = $(CFLAGS) #-m64 # -O3

TARGETS = secondpass.so

all: $(TARGETS)

secondpass.so: $(DEPS)
	$(CXX) $(CFLAGS64) $(INCLUDE_DIRS) -shared -fPIC $(SRCS) -o secondpass.so -ldl -lpthread

clean:
	rm -f $(TARGETS)
