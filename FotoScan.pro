QT += widgets

HEADERS       = scanner.hpp \
                detection.hpp \
                clip.hpp \
                viewer.hpp \
                graphicsview.hpp
SOURCES       = main.cpp \
                scanner.cpp \
                detection.cpp \
                clip.cpp \
                viewer.cpp \
                graphicsview.cpp

QMAKE_CXXFLAGS += -fopenmp
LIBS += -fopenmp

CONFIG += link_pkgconfig
PKGCONFIG += opencv
