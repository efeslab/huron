CFLAGS += -I../include -g -O2
CXXFLAGS += -std=c++11 -I../include/ -g -O2
LIBS += -ldl -lelf -lrt -lpthread

bin_PROGRAMS = tmildr tmidetector tmialloc llalloc
lib_LTLIBRARIES = libtmi.la libtmiprotect.la libtmialloc.la libllalloc.la

tmialloc_SOURCES = tmialloc.cpp

llalloc_SOURCES = llalloc.cpp

tmildr_SOURCES = tmildr.cpp tmilib.cpp
tmildr_CXXFLAGS = -DTMI_PROTECT

tmidetector_SOURCES = tmidetector.cpp

libllalloc_la_SOURCES = ll_alloc_original.c

libtmi_la_SOURCES = detector.cpp perf.cpp libtmi.cpp tmithread.cpp hooks.cpp tmiutil.cpp tmimem.cpp ll_alloc.c

libtmiprotect_la_SOURCES = detector.cpp perf.cpp libtmi.cpp tmimem.cpp ll_alloc.c tmithread.cpp tmisync.cpp hooks.cpp tmiutil.cpp internalmem.cpp timers.cpp
#libtmiprotect_la_SOURCES = detector.cpp perf.cpp libtmi.cpp tmimem.cpp ll_alloc.c tmithread.cpp tmiutil.cpp internalmem.cpp
libtmiprotect_la_CXXFLAGS = -DTMI_PROTECT

libtmialloc_la_SOURCES = tmimem.cpp ll_alloc.c internalmem.cpp tmisync.cpp hooks.cpp tmiutil.cpp
