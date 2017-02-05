---------------------------------------------
--SLIC3R: STANDALONE FORK--------------------
---------------------------------------------

This is a fork of Slic3r, with two main purposes:

    -it can be compiled with CMake into a command-line application that uses Slic3r's excellent code base to slice a 3D mesh.
     This application uses stdin and stdout to communicate with a controller application that processes the slices.
    
    -it can be compiled as usual to produce a modified version of Slic3r with a command-line option to accept as input a file
     that contains several sets of slices in a custom format, where each set of slices is intended to be printed with a different
     extruder. In this way, Slic3r can be used as a backend to do optimized infilling, generation of support material,
     reprap gcode generation, an in general combine an awesome piece of software with slice processing.


---------------------------------------------
--COMPILING COMMAND-LINE STANDALONE SLICER---
---------------------------------------------


In Windows, Slic3r's C++ sources do not compile at the moment in Visual Studio, so mingw-64 is used. This project can also be compiled in Linux/GCC.

steps:

1. If it is not already at ../clipper, download or clone git repo at https://github.com/jdfr/clipper to have correctly placed the following files:
      ../clipper/allocation_schemes.hpp
      ../clipper/clipper.hpp
      ../clipper/clipper.cpp
      ../clipper/iopaths/common.hpp
      ../clipper/iopaths/iopaths.hpp
      ../clipper/iopaths/iopaths.cpp

2. If it is not already at ../boost, put a copy of boost there (the originally used version is a stripped down copy of Boost 1.055)

3. Create and go to subdir bin
  mkdir bin
  cd bin

4. In windows, you will need mingw-64. It comes with many third-party projects, such as Perl (strawberry perl) or Python (winpython) distributions. You can also download a portable, standalone copy of mingw-64, put it on a folder, and set the PATH so it is visible. For example, if you download http://sourceforge.net/projects/perlmingw/ you will need to also set the path in this way:
  set SET PATH=%PATH%;PATH_TO_MINGW_64\bin

5. Use cmake to generate a makefile
  cmake .. -G "MinGW Makefiles"

6. Run make
  cmake --build .

7. In windows, if you need to move the executables, please take with it the C++ runtime DLLs from minw-64's bin directory.

8. Enjoy!

---------------------------------------------
--COMPILING MODIFIED VERSION OF SLIC3R-------
---------------------------------------------

Currently, the libslic3r sources are out of sync with respect to the version of ClipperLib hard-coded into the Slic3r repository, so the project cannot be compiled outright. To fix the compilation process, do the following:
  -remove the following files:
    *Slic3r/xs/src/clipper.hpp
    *Slic3r/xs/src/clipper.cpp
  -if not already downloaded for the command-line version, download modified ClipperLib repository from https://github.com/jdfr/clipper
  -from the downloaded repository, copy the following files to the folder Slic3r/xs/src:
    *clipper/clipper/allocation_schemes.hpp
    *clipper/clipper/clipper.hpp
    *clipper/clipper/clipper.cpp
  
After fixing that, compile as usual with the following commands:

perl Build.PL
perl Build.PL --gui      (if you want to have the Slic3r GUI)

The functionality to directly accept slices can be used from the command line (not the GUI):

perl slic3r.pl --load YOUR_CONFIGURATION.ini --import-paths=YOUR_SLICES_FILE.paths x

The 'x' at the end is required.

The configuration file YOUR_CONFIGURATION.ini should be prepared with the same nozzles as the ones used for generating the *.paths file.

ALSO, VERY IMPORTANT: all configuration parameters relative to filament dimensions (especially the different *_width parameters) MUST NOT BE IN ABSOLUTE TERMS, BUT BE LEFT AS DEFAULT (0) OR CONFIGURED AS PERCENTAGES. Otherwise, slic3r will likely error out, or hang the computer.

The slices file YOUR_SLICES_FILE.paths is the file to read the slices from. The resulting gcode will be in YOUR_SLICES_FILE.gcode.

If you want a Windows redistributable executable, you have lots of options, such as downloading pp from CPAN and doing

pp slic3r.pl -x -o slic3r.exe -M Method::Generate::BuildAll

This will generate an executable named slic3r.exe, which can be used from the command line like this:

slic3r.exe --load YOUR_CONFIGURATION.ini --import-paths=YOUR_SLICES_FILE.paths x
