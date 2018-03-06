# SLIC3R: STANDALONE FORK #

This is a fork of Slic3r, with two main purposes:

* it can be compiled with CMake into a command-line application that uses Slic3r's excellent code base to just slice a 3D mesh, i.e. compute just the contours, not the toolpaths. This application uses stdin to receive commands and stdout (in binary mode) to return the results, so it can be used by other applications to compute contours from 3D meshes.
    
* it can be compiled as usual to produce a modified version of Slic3r with a command-line option to accept as input a file that contains several sets of slices in a custom format, where each set of slices is intended to be printed with a different set of options, such as a different extruder or a different flow rate. In this way, Slic3r can be used as a backend to do optimized infilling, generation of support material, reprap gcode generation, an in general use an awesome piece of software for toolpath generation.



### COMPILING COMMAND-LINE STANDALONE SLICER ###


In Windows, Slic3r's C++ sources do not compile easily in Visual Studio, so mingw-64 is pretty much required (it is NOT intended to be compiled in 32-bit mode!). This project can also be compiled in Linux/GCC.

steps:

1. If it is not already at ../clipper, download or clone git repo at https://github.com/jdfr/clipper to have correctly placed the following files:
```
slic3r_base_dir/../clipper/clipper/allocation_schemes.hpp
slic3r_base_dir/../clipper/clipper/clipper.hpp
slic3r_base_dir/../clipper/clipper/clipper.cpp
```
2. In windows, you will need mingw-64 with a reasonably recent version (gcc 6.3.0 is known to work). It comes with many third-party projects, such as Perl (strawberry perl) or Python (winpython) distributions. You can also download a portable, standalone copy of mingw-64, put it on a folder, and set the PATH so it is visible. For example, if you download http://sourceforge.net/projects/perlmingw/ you will need to also set the path in this way:
```
SET PATH=%PATH%;PATH_TO_MINGW_64\bin
```
If you are also going to compile the modified (non-standalone) version of Slic3r, in Windows you'll have to install Slic3r Perl. See Slic3r's [Running Slic3r from git](https://github.com/alexrj/Slic3r/wiki/Running-Slic3r-from-git-on-Windows) wiki pages), which will come with a version of mingw-64. Slic3r Perl will add this mingw-64 to the system PATH, so you'll not have to modify it. 

2. If you do not have boost, download a copy (at least version 1.63). In linux, the easiest way is to use your distro's package manager. If you are on windows, or want anyway to compile from source, best advice is to follow the recommendations from Slic3r's [Running Slic3r from git](https://github.com/alexrj/Slic3r/wiki/Running-Slic3r-from-git-on-Windows) wiki pages, especially the bit about placing it on c:\dev\boost_1_63, because Slic3r's build script is not always able to use the environment variable BOOST_DIR (not sure why, but it happened to me!). Be sure to remove or rename any previous version of boost in c:\dev (or even any compressed file named boost*), because Slic3r's build script may not use the desired one! When you have downloaded it, compile it with mingw-64 (follow the instructions in the aferomentioned Slic3r's wiki page).

3. Create and go to subdir bin
```
mkdir bin
cd bin
```

5. Use cmake to generate a makefile 
```
cmake ..                      #linux
cmake .. -G "MinGW Makefiles" :windows
#add the following arguments to the cmake invocation if you are on windows or your copy of boost is not system-wide:
# -D BOOST_ROOT="path/to/boost/root/directory"
#In Windows, also add -D Boost_USE_STATIC_LIBS=ON if you are using the [new instructions](https://github.com/alexrj/Slic3r/wiki/Running-Slic3r-from-git-on-Windows) from Slic3r's wiki to use a custom Slic3r Perl and compiling Boost to static libs.
```

6. Run make
```
cmake --build .
```

7. In windows, if you need to move the executables, please take with them the C++ runtime DLLs from minw-64's bin directory. I do not know if there are license issues about distributing these, though.

8. Enjoy!

### COMPILING MODIFIED VERSION OF SLIC3R ###

The libslic3r sources are adapted to use a version of ClipperLib different from the one hard-coded into the Slic3r repository, so the project cannot be compiled outright. To fix the compilation process, do the following:

* remove the following files:
```
Slic3r/xs/src/clipper.hpp
Slic3r/xs/src/clipper.cpp
```
* if not already downloaded for the command-line version, download the modified ClipperLib repository from https://github.com/jdfr/clipper
* from the downloaded repository, copy the following files to the folder Slic3r/xs/src:
```
clipper/clipper/allocation_schemes.hpp
clipper/clipper/clipper.hpp
clipper/clipper/clipper.cpp
```
  
After fixing that, compile as usual with the following commands:

```
perl Build.PL
perl Build.PL --gui #(if you want to have the Slic3r GUI)
```

The functionality to directly accept slices can be used from the command line (not the GUI):

```
perl slic3r.pl --load YOUR_CONFIGURATION.ini --import-paths=YOUR_SLICES_FILE.paths x
```

The 'x' at the end is required.

The configuration file YOUR_CONFIGURATION.ini should be prepared with the same nozzles as the ones used for generating the *.paths file.

ALSO, VERY IMPORTANT: all configuration parameters relative to filament dimensions (especially the different *_width parameters) MUST NOT BE IN ABSOLUTE TERMS, BUT BE LEFT AS DEFAULT (0) OR CONFIGURED AS PERCENTAGES. Otherwise, slic3r will likely error out, or hang the computer. In general, Slic3r does lots of sanity checks, but many of them are disabled when using the --import-paths flag!

The file YOUR_SLICES_FILE.paths is the file to read the slices from. The resulting gcode will be in YOUR_SLICES_FILE.gcode.

If you want a Windows redistributable executable, you have lots of options, such as downloading `pp` from CPAN and doing

```
pp slic3r.pl -x -o slic3r.exe -M Method::Generate::BuildAll
```

This will generate an executable named slic3r.exe, which can be used from the command line like this:

```
slic3r.exe --load YOUR_CONFIGURATION.ini --import-paths=YOUR_SLICES_FILE.paths x
```

Another option is to use Slic3r's built-in windows packager. It's your call...
