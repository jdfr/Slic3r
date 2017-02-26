CMAKE_MINIMUM_REQUIRED(VERSION 2.8.0)

set(SRC ${CMAKE_CURRENT_SOURCE_DIR})

option(ENABLE_SLICER_LOGGING   "enable slicer debug output to file" ON)
option(CLIPPER_USE_ARENA       "enable ArenaMemoryManager for ClipperLib" OFF)
set(INITIAL_ARENA_SIZE "52428800" CACHE STRING "Initial size for ArenaMemoryManager")
#1-5 MB: 1048576 2097152 3145728 4194304 5242880
set(BIGCHUNK_ARENA_SIZE "2097152" CACHE STRING "Initial size for ArenaMemoryManager")

set(CLIPPER_BASE_DIR "" CACHE PATH "tweaked clipper/iopaths base dir")
if("${CLIPPER_BASE_DIR}" STREQUAL "")
  if (EXISTS ${SRC}/../clipper)
    set(CLIPPER_BASE_DIR ${SRC}/../clipper)
  else()
    message(FATAL_ERROR "Please set the variable CLIPPER_BASE_DIR to the base path to the clipper/iopaths directory")
  endif()
endif()

set(clipper_include_dir ${CLIPPER_BASE_DIR}/clipper)
set(iopaths_include_dir ${CLIPPER_BASE_DIR}/iopaths)
set(clipper_source ${clipper_include_dir}/clipper.cpp)
set(clipper_header ${clipper_include_dir}/clipper.hpp)
set(alloc_header   ${clipper_include_dir}/allocation_schemes.hpp)
set(iopaths_source ${iopaths_include_dir}/iopaths.cpp)
set(iopaths_header ${iopaths_include_dir}/iopaths.hpp)
set(common_header  ${iopaths_include_dir}/common.hpp)

if(NOT EXISTS ${clipper_source} OR NOT EXISTS ${iopaths_source} OR NOT EXISTS ${clipper_header} OR NOT EXISTS ${iopaths_header} OR NOT EXISTS ${common_header} OR NOT EXISTS ${alloc_header})
  message(FATAL_ERROR "tweaked clipper/iopaths base dir does not contain the expected files and directories!!!")
endif()

PROJECT(slic3r)

if(NOT(CMAKE_SIZEOF_VOID_P EQUAL 8))
  message(FATAL_ERROR "This code is designed only for x64 systems! Size of void pointer: " ${CMAKE_SIZEOF_VOID_P})
endif()

set(BOOST_ROOT                "" CACHE PATH "path prefix to boost library")
find_package(Boost 1.59.0 QUIET COMPONENTS thread system)
if (NOT Boost_FOUND)
      message(FATAL_ERROR "Could not find BOOST. You can set BOOST_ROOT to the base directory of BOOST to provide a hint of its location")
endif()

SET(CMAKE_BUILD_TYPE "Release" CACHE STRING "Release type")

#add compiler flags from Slicer/xs/Build.PL (HAS_BOOL and SLIC3RXS
#are specific to the perl build, so they are not included here)
# _GLIBCXX_USE_C99 : to get the long long type for g++
# NOGDI            : prevents inclusion of wingdi.h which defines functions Polygon() and Polyline() in global namespace
ADD_DEFINITIONS(-DSLIC3R_STANDALONE_TOOL -D_GLIBCXX_USE_C99 -DNOGDI)# -DDEBUG_IOPATHS)

#hope that compiling the code under c++11 brings free performance gains
#we -std=gnu++11 instead of -std=c++11 because the code breaks on usage of M_PI otherwise (it is not defined in c++11 apparently)
set(CMAKE_CXX_FLAGS ${CMAKE_CXX_FLAGS} "-std=gnu++11") 

if (CLIPPER_USE_ARENA)
  ADD_DEFINITIONS(-DCLIPPER_USE_ARENA -DINITIAL_ARENA_SIZE=${INITIAL_ARENA_SIZE} -DBIGCHUNK_ARENA_SIZE=${BIGCHUNK_ARENA_SIZE})
endif()

set(MASK "/*.c*") #*.c, *.cc, *.cpp
file(GLOB_RECURSE poly2tri_c  "${SRC}/xs/src/poly2tri${MASK}") 
file(GLOB_RECURSE admesh_c    "${SRC}/xs/src/admesh${MASK}")
set(slicer_source "${SRC}/standalone/slicer.cpp")
set(bb_source "${SRC}/standalone/bb.cpp")
set(zpos_source "${SRC}/standalone/zpos.cpp")
#include just the sources we need to make it compile. Much code could be culled but for now we don't bother to edit the files
set (selectedfiles BoundingBox ClipperUtils ExPolygon ExPolygonCollection Geometry Line MultiPoint Point Polygon Polyline PolylineCollection Surface TriangleMesh utils)
set(slic3r_c "")
foreach(f ${selectedfiles})
  list(APPEND slic3r_c ${SRC}/xs/src/libslic3r/${f}.cpp)
endforeach()
list(APPEND slic3r_c ${SRC}/xs/src/polypartition.cpp)

set(slic3r_sources ${admesh_c} ${slic3r_c} ${poly2tri_c})

set (INCLUDE_DIRS ${clipper_include_dir} ${iopaths_include_dir} ${Boost_INCLUDE_DIRS} ${SRC}/xs/src ${SRC}/standalone ${SRC}/xs/src/admesh ${SRC}/xs/src/libslic3r ${SRC}/xs/src/poly2tri)
include_directories(${INCLUDE_DIRS})

ADD_LIBRARY(slic3r SHARED ${clipper_source} ${iopaths_source} ${slic3r_sources})

if (CMAKE_COMPILER_IS_GNUCC)
  target_compile_options(slic3r PUBLIC "-Wno-deprecated-declarations")
endif()

target_link_libraries(slic3r ${Boost_LIBRARIES})

ADD_EXECUTABLE(slicer ${slicer_source})
target_link_libraries(slicer ${Boost_LIBRARIES} slic3r)
if (ENABLE_SLICER_LOGGING)
    set_property(TARGET slicer APPEND PROPERTY COMPILE_DEFINITIONS "SLICER_USE_DEBUG_FILE")
endif()


ADD_EXECUTABLE(bb ${bb_source})
target_link_libraries(bb slic3r)

ADD_EXECUTABLE(zpos ${zpos_source})
target_link_libraries(zpos slic3r)

if(${MINGW})
  if (NOT MINGW_DLLS_COPIED)
    if (CMAKE_RUNTIME_OUTPUT_DIRECTORY)
      set(DLL_OUTPUTDIR ${CMAKE_RUNTIME_OUTPUT_DIRECTORY})
    else()
      set(DLL_OUTPUTDIR ${CMAKE_CURRENT_BINARY_DIR})
    endif()
    get_filename_component( MINGWPATH ${CMAKE_CXX_COMPILER} PATH )
    file(GLOB DLL_FILES ${MINGWPATH}/*.dll)
    file(COPY ${DLL_FILES} DESTINATION ${DLL_OUTPUTDIR})
    set(MINGW_DLLS_COPIED "True")
  endif()
endif()
