.PHONY: default
default: release

LDLIBS   += -lopencv_core -lopencv_highgui -lopencv_imgproc -lopencv_objdetect

CPPFLAGS += -Wall
CXXFLAGS += -std=c++11

main: clip.o

.PHONY: debug release
debug: CXXFLAGS += -DDEBUG -g -fno-omit-frame-pointer -O2
debug: main
release: CXXFLAGS += -DNDEBUG -O3 -fopenmp
release: main

.PHONY: clean
clean:
	$(RM) main clip.o

.PHONY: format
format:
	clang-format -i $(wildcard *.hpp) $(wildcard *.cpp)