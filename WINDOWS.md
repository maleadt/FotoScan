1) Qt

- https://www.qt.io/download-open-source/
- Qt 5.7, unselect all options (except `MinGW`)
- Tools, Qt Creator


2) OpenCV

- http://opencv.org/
- OpenCV 3.2, OpenCV for Windows
- extract somewhere, eg. C:\OpenCV

We now need to build our own MinGW-compatible binaries, as the default ones are for MSVC...

2a) get CMake

- https://cmake.org/download/
- Install, do not add to path

2b) build OpenCV

- add C:\Qt\Tools\mingw530_32\bin to path (adapt if needed)
- create directory C:\OpenCV\build_mingw
- start CMake GUI
  - sources C:\OpenCV\sources
  - binary dir C:\OpenCV\build_mingw
  - click Configure
  - specify mingw makefiles, native compilers
  - uncheck WITH_TBB, WITH_IPP, WITH_CUDA
  - click 'configure' until red lines are gone, then 'generate'
- navigate with command line to C:\OpenCV\build_mingw
  - better use something decent, ie. 'cmder'
  - execute 'mingw32-make install'


3) FotoScan

- https://github.com/maleadt/FotoScan
- Clone or download, Download ZIP
- open FotoScan.pro with QtCreator, "configure project"
- change the build type to Release (4th button from the bottom left corner, else it'll be dead slow)
- double-click FotoScan.pro, replace CONFIG and PKGCONFIG lines concerning OpenCV with:
  ```
  win32 {
    OPENCV_ROOT = C:/OpenCV/build_mingw/install
    OPENCV_VER = 320
  
    INCLUDEPATH += $${OPENCV_ROOT}/include
    LIBS += -L$${OPENCV_ROOT}/x86/mingw/lib
  
    CONFIG(release, debug|release) {
        LIBS += -llibopencv_core$${OPENCV_VER}
        LIBS += -llibopencv_imgproc$${OPENCV_VER}
        LIBS += -llibopencv_objdetect$${OPENCV_VER}
    }
    CONFIG(debug, debug|release) {
        DEFINES += DEBUG_MODE
        LIBS += -llibopencv_core$${OPENCV_VER}d
        LIBS += -llibopencv_imgproc$${OPENCV_VER}d
        LIBS += -llibopencv_objdetect$${OPENCV_VER}d
    }
  }
  ```
  ... adjusting the OpenCV path and DLL name accordingly.
  - hit the 'build' button (last button from the bottom left corner)
  - navigate to the build folder (next to the source tree, "build-FotoScan-...-Release", verify it's a release build!)
  - copy required DLLs (try running FotoScan.exe, it'll complain) to the EXE's folder
  - run `FotoScan.exe --help`, afterwards `FotoScan.exe $INPUT
