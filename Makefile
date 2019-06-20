
# This is what I use, uncomment if you know your arch and want to specify
# ARCH= -gencode arch=compute_52,code=compute_52

#OPENCV=1
#only for linker
# ifdef OPENCV
#   COMMON += -DOPENCV `pkg-config --cflags opencv`
#   LDFLAGS += `pkg-config --libs opencv` -lstdc++
# endif

#OPENCV=-I/home/developer/opencvs/OpenCV-4.1.0/bin/include/opencv4/ /home/developer/opencvs/OpenCV-4.1.0/bin/lib/libopencv_calib3d.so -lopencv_calib3d /home/developer/opencvs/OpenCV-4.1.0/bin/lib/libopencv_contrib.so -lopencv_contrib /home/developer/opencvs/OpenCV-4.1.0/bin/lib/libopencv_core.so -lopencv_core /home/developer/opencvs/OpenCV-4.1.0/bin/lib/libopencv_features2d.so -lopencv_features2d /home/developer/opencvs/OpenCV-4.1.0/bin/lib/libopencv_flann.so -lopencv_flann /home/developer/opencvs/OpenCV-4.1.0/bin/lib/libopencv_gpu.so -lopencv_gpu /home/developer/opencvs/OpenCV-4.1.0/bin/lib/libopencv_highgui.so -lopencv_highgui /home/developer/opencvs/OpenCV-4.1.0/bin/lib/libopencv_imgproc.so -lopencv_imgproc /home/developer/opencvs/OpenCV-4.1.0/bin/lib/libopencv_legacy.so -lopencv_legacy /home/developer/opencvs/OpenCV-4.1.0/bin/lib/libopencv_ml.so -lopencv_ml /home/developer/opencvs/OpenCV-4.1.0/bin/lib/libopencv_objdetect.so -lopencv_objdetect /home/developer/opencvs/OpenCV-4.1.0/bin/lib/libopencv_ocl.so -lopencv_ocl /home/developer/opencvs/OpenCV-4.1.0/bin/lib/libopencv_photo.so -lopencv_photo /home/developer/opencvs/OpenCV-4.1.0/bin/lib/libopencv_stitching.so -lopencv_stitching /home/developer/opencvs/OpenCV-4.1.0/bin/lib/libopencv_superres.so -lopencv_superres /home/developer/opencvs/OpenCV-4.1.0/bin/lib/libopencv_ts.so -lopencv_ts /home/developer/opencvs/OpenCV-4.1.0/bin/lib/libopencv_video.so -lopencv_video /home/developer/opencvs/OpenCV-4.1.0/bin/lib/libopencv_videostab.so -lopencv_videostab

CC=gcc
CPP=g++

#common to both compiler and linker
COMMON=-Iinclude/ -I. -I -DSPDLOG_COMPILED_LIB

#only for compiler
CFLAGS=-Wall -Wno-unused-result -Wno-unknown-pragmas -Wfatal-errors -O0 -g
CPPFLAGS=-std=c++11
LDFLAGS= -lmvnc -lm -ljpeg -lturbojpeg -lpthread

DEPS=$(wildcard *.hpp)
DEPS+=holooj.h

OBJSDIR=./debug/objs/
OBJ=spdlog.o ncs.o ny2.o coordinator.o main.o
OBJS=$(addprefix $(OBJSDIR), $(OBJ))
EXEC=./debug/gengi

$(info $(OBJS))


all: $(OBJS)
	$(CPP)  $(COMMON) $(CFLAGS) $(CPPFLAGS) $^ -o gengi $(LDFLAGS)

	
$(OBJSDIR)%.o: %.cpp $(DEPS)
	$(CPP) $(COMMON) $(CFLAGS) $(CPPFLAGS) -c $< -o $@

.PHONY: clean
clean:
	rm -rf debug/objs/* gengi
	mkdir -p ./debug/objs/