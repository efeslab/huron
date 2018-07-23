SOURCE_DIR = .
INCLUDE_DIR = .

SRCS =  $(SOURCE_DIR)/main.cpp

INCS =  $(INCLUDE_DIR)/Utils.h ./Layout.h

DEPS = $(SRCS) $(INCS)

CXX = clang++

CFLAGS   = -std=c++1z -g -O3 -I$(INCLUDE_DIR) -fno-omit-frame-pointer -DDEBUG

TARGETS = postprocess

all: $(TARGETS)

postprocess: $(DEPS)
	$(CXX) $(CFLAGS) $(SRCS) -o postprocess -lpthread

clean:
	rm -f $(TARGETS)
