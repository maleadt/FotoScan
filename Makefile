CC=clang
CXX=clang++

LDLIBS = -lopencv_core -lopencv_highgui -lopencv_imgproc -lopencv_objdetect

CPPFLAGS += -Wall
CXXFLAGS += -std=c++11 -g

.PHONY: all clean

all: main

clean:
	$(RM) main
