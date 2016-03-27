CC=clang
CXX=clang++

LDLIBS = -lopencv_core -lopencv_highgui -lopencv_imgproc

CPPFLAGS += -Wall
CXXFLAGS += -std=c++11

.PHONY: all clean

all: main

clean:
	$(RM) main
