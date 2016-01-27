CMAKE_MINIMUM_REQUIRED(VERSION 2.8.0)

set(SRC ${CMAKE_CURRENT_SOURCE_DIR})

option(ENABLE_SLICER_LOGGING   "enable slicer debug output to file" ON)

if(EXISTS ${SRC}/../clipper)
  set(CLIPPER_BASE_DIR ${SRC}/../clipper)
else()
  #as far as I understand, this will enable users of cmake-gui to set this directory
  set(CLIPPER_BASE_DIR "" CACHE PATH "tweaked clipper/iopaths base dir")
endif()

if(EXISTS ${SRC}/../boost/include/boost)
  set(BOOST_INCLUDE_DIR ${SRC}/../boost/include)
else()
  #as far as I understand, this will enable users of cmake-gui to set this directory
  set(BOOST_INCLUDE_DIR "" CACHE PATH "path to boost library")
endif()

set(clipper_include_dir ${CLIPPER_BASE_DIR}/clipper)
set(iopaths_include_dir ${CLIPPER_BASE_DIR}/iopaths)
set(clipper_source ${clipper_include_dir}/clipper.cpp)
set(clipper_header ${clipper_include_dir}/clipper.hpp)
set(iopaths_source ${iopaths_include_dir}/iopaths.cpp)
set(iopaths_header ${iopaths_include_dir}/iopaths.hpp)
set(common_header  ${iopaths_include_dir}/common.hpp)

if(NOT EXISTS ${clipper_source} OR NOT EXISTS ${iopaths_source} OR NOT EXISTS ${clipper_header} OR NOT EXISTS ${iopaths_header} OR NOT EXISTS ${common_header})
  message(FATAL_ERROR "tweaked clipper/iopaths base dir does not contain the expected files and directories!!!")
endif()

PROJECT(slic3r)

if (NOT CMAKE_BUILD_TYPE)
  SET(CMAKE_BUILD_TYPE "Release" CACHE STRING "Release type")
endif()

#add compiler flags from Slicer/xs/Build.PL (HAS_BOOL and SLIC3RXS
#are specific to the perl build, so they are not included here)
# _GLIBCXX_USE_C99 : to get the long long type for g++
# NOGDI            : prevents inclusion of wingdi.h which defines functions Polygon() and Polyline() in global namespace
ADD_DEFINITIONS(-D_GLIBCXX_USE_C99 -DNOGDI)# -DDEBUG_IOPATHS)

option(FULLLIBRARY       "include all sources in the library" OFF)

set(MASK "/*.c*") #*.c, *.cc, *.cpp
file(GLOB_RECURSE poly2tri_c  "${SRC}/poly2tri${MASK}") 
file(GLOB_RECURSE admesh_c    "${SRC}/admesh${MASK}")
set(slicer_source "${SRC}/slicer.cpp")
set(bb_source "${SRC}/bb.cpp")
set(zpos_source "${SRC}/zpos.cpp")
if (FULLLIBRARY)
  file(GLOB_RECURSE slic3r_c "${SRC}/libslic3r${MASK}")
else()
  #include just the sources we need to make it compile. Much code could be culled but for now we don't bother to edit the files
  set (selectedfiles BoundingBox ClipperUtils ExPolygon ExPolygonCollection Geometry Line MultiPoint Point Polygon Polyline PolylineCollection TriangleMesh utils polypartition)
  set(slic3r_c "")
  foreach(f ${selectedfiles})
    list(APPEND slic3r_c ${SRC}/libslic3r/${f}.cpp)
  endforeach()
endif()

set(slic3r_sources ${admesh_c} ${slic3r_c} ${poly2tri_c})

set (INCLUDE_DIRS ${clipper_include_dir} ${iopaths_include_dir} ${BOOST_INCLUDE_DIR} ${SRC} ${SRC}/admesh ${SRC}/libslic3r ${SRC}/poly2tri)
include_directories(${INCLUDE_DIRS})

ADD_LIBRARY(slic3r SHARED ${clipper_source} ${iopaths_source} ${slic3r_sources})

ADD_EXECUTABLE(slicer ${slicer_source})
target_link_libraries(slicer slic3r)
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
