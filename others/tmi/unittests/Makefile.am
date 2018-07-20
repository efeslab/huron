GOOGLETEST_DIR=/home/delozier/googletest

CXXFLAGS += -I$(GOOGLETEST_DIR)/include/ -I../include/ -std=c++11
LDFLAGS = -pthread
LIBS = $(GOOGLETEST_DIR)/libgtest.a ../build/lib/libtmiprotect.la

TESTS = hooktest synctest memorytest

check_PROGRAMS = hooktest synctest memorytest

hooktest_SOURCES = hooksuite.cpp main.cpp 
synctest_SOURCES = syncsuite.cpp main.cpp
memorytest_SOURCES = memorysuite.cpp main.cpp
