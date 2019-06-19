
# This is what I use, uncomment if you know your arch and want to specify
# ARCH= -gencode arch=compute_52,code=compute_52

#OPENCV=1
#only for linker
# ifdef OPENCV
#   COMMON += -DOPENCV `pkg-config --cflags opencv`
#   LDFLAGS += `pkg-config --libs opencv` -lstdc++
# endif

CC=gcc
CPP=g++

#common to both compiler and linker
COMMON=-Iinclude/ -I.

#only for compiler
CFLAGS=-Wall -Wno-unused-result -Wno-unknown-pragmas -Wfatal-errors -O0 -g
CPPFLAGS=-std=c++11
LDFLAGS= -lmvnc -lm -ljpeg -lturbojpeg -lpthread

EXEC=./debug/gengi



all: debug/objs/spdlog.o debug/objs/ncs.o debug/objs/ny2.o debug/objs/coordinator.o debug/objs/main.o holooj.h
	$(CPP)  $(COMMON) $(CFLAGS) $(CPPFLAGS) $^ -o gengi $(LDFLAGS)

debug/objs/%.o: %.cpp %.hpp
	$(CPP) $(COMMON) $(CFLAGS) $(CPPFLAGS) -c $< -o $@

debug/objs/%.o: %.cpp
	$(CPP) $(COMMON) $(CFLAGS) $(CPPFLAGS) -c $< -o $@

.PHONY: clean
clean:
	rm -rf debug/objs/* gengi
	mkdir -p ./debug/objs/

