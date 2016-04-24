CC=clang
CXX=clang++

LDLIBS = -lopencv_core -lopencv_highgui -lopencv_imgproc -lopencv_objdetect

CPPFLAGS += -Wall
CXXFLAGS += -std=c++11 -g

.PHONY: all clean format

main: clip.o

all: main

clean:
	$(RM) main clip.o

format:
	clang-format -i $(wildcard *.hpp) $(wildcard *.cpp)