Slic3r's C++ sources do not compile at the moment in Visual Studio, so mingw-64 is used. This project can also be compiled in Linux/GCC.

steps:

1. If it is not already at ../clipper, download or clone git repo at https://github.com/jdfr/clipper to have correctly placed the following files:
      ../clipper/clipper.hpp
      ../clipper/clipper.cpp
      ../clipper/iopaths/common.hpp
      ../clipper/iopaths/iopaths.hpp
      ../clipper/iopaths/iopaths.cpp

2. If it is not already at ../boost, put a copy of boost there (the originally used version is a stripped down copy of Boost 1.055)

3. create and go to subdir bin
  mkdir bin
  cd bin

4. in windows, if necessary, download and put on a folder a mingw distribution, for example: http://sourceforge.net/projects/perlmingw/

5. in windows, if necessary, set the path to make mingw work (create or modify setpath.bat accordingly!)
  set SET PATH=%PATH%;PATH_MINGW_64_BIN_DIR

6. use cmake to generate a makefile
  cmake .. -G "MinGW Makefiles"

7. run make
  cmake --build .

8. in windows, if you need to move the executables, please take with it the dlls

9. enjoy!

NOTE: In Windows, if ./setpath.bat sets correctly the path to mingw-64, mk.bat will do steps 4, 5, 6 and 7 automatically, but requires ./bin to not be created yet (otherwise, step 5 can be replayed by setpath.bat and steps 6 and 7 by rmk.bat)
