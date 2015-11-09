//#define _CRT_SECURE_NO_WARNINGS


#include "TriangleMesh.hpp"
#include "ClipperUtils.hpp"
#include "iopaths.hpp"
#include <string.h>
#include <stdio.h>
#include <fcntl.h>

//#define BUFSIZE (1024*256*8)

#define DEBUG_MAIN
//#define BENCHMARK
#define ENSURE_REPAIR

#if defined(DEBUG_MAIN) || defined(BENCHMARK)
#define USE_DEBUG_FILE
#  define CLOSE_DEBUG_FILE {fclose(STDERR);}
#else
#  define CLOSE_DEBUG_FILE {}
#endif

#ifdef DEBUG_MAIN
#  define DEBUGPRINTF(...) {fprintf(STDERR, __VA_ARGS__); fflush(STDERR);}
#else
#  define DEBUGPRINTF(...) {}
#endif

#ifndef INWINDOWS
#  undef BENCHMARK
#endif

#ifdef BENCHMARK 
#  include <windows.h>
#  define BENCHGETTICK(tick)                 QueryPerformanceCounter(&(tick));
#  define BENCHPRINTF(...)                   {fprintf(STDERR, __VA_ARGS__);}
#  define SHOWBENCHMARK(string, tInit, tEnd) {fprintf(STDERR, string, (double)((tEnd.QuadPart - tInit.QuadPart) * 1.0 / frequency.QuadPart));}
#else
#  define BENCHGETTICK(...)                  {}
#  define BENCHPRINTF(...)                   {}
#  define SHOWBENCHMARK(...)                 {}
#endif

size_t io_num_bytes(clp::Paths &paths, PathCloseMode mode) {
  size_t n = 1 + paths.size();
  for(clp::Paths::iterator path = paths.begin(); path!=paths.end(); path++) {
      n += 2*path->size();
  }
  if ((mode == PathLoop) && (paths.size() > 0)) {
    n += 2;
  }
  return n*8;
}

size_t io_num_bytes_total(std::vector<clp::Paths> &pathss, PathCloseMode mode) {
  size_t n = 0;
  for(std::vector<clp::Paths>::iterator paths = pathss.begin(); paths!=pathss.end(); paths++) {
      n += io_num_bytes(*paths, mode);
  }
  return n;
}

int64 getCommand(FILE *input) {
  int64 command;
  READ_BINARY(&command, sizeof(int64), 1, input);
  return command;
}


int main(int argc, char **argv) {

  if (argc<2) {
    fprintf(stderr, "%d IS AN INCORRECT NUMBER OF ARGUMENTS!!!", argc-1);
    return -1;
  }
  char *filename=argv[1];
  
  Slic3r::TriangleMesh *mesh = new Slic3r::TriangleMesh();
  mesh->stl.err = stderr;

  mesh->ReadSTLFile(filename);
  
  if (mesh->needed_repair()) {
    mesh->repair();
  }

  Slic3r::BoundingBoxf3 bb = mesh->bounding_box();
  
  printf("bounding box:\n");
  printf("X: %f %f\n", bb.min.x, bb.max.x);
  printf("Y: %f %f\n", bb.min.y, bb.max.y);
  printf("Z: %f %f\n", bb.min.z, bb.max.z);
  
  delete mesh; 
  return 0;
}
