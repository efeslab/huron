SRCS = Detect.cpp main.cpp Repair.cpp Utils.cpp

INCS = Detect.h Repair.h Stats.h Utils.h

DEPS = $(SRCS) $(INCS)

CXX = clang++

CFLAGS = -std=c++1z -g -O3

TARGETS = postprocess

all: $(TARGETS)

postprocess: $(DEPS)
	$(CXX) $(CFLAGS) $(SRCS) -o postprocess -lpthread

clean:
	rm -f $(TARGETS)
