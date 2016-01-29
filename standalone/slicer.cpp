//#define _CRT_SECURE_NO_WARNINGS


#include "TriangleMesh.hpp"
#include "ClipperUtils.hpp"
#include "iopaths.hpp"
#include <string.h>
#include <stdio.h>
#include <fcntl.h>
#if defined(_WIN32) || defined(_WIN64)
#  define INWINDOWS
#  include <io.h>
#endif

//#define BUFSIZE (1024*256*8)

#define SLICER_DEBUG_MAIN
//#define SLICER_BENCHMARK
#define ENSURE_REPAIR

#ifndef INWINDOWS
#  undef SLICER_BENCHMARK
#endif

#if defined(SLICER_DEBUG_MAIN) || defined(SLICER_BENCHMARK)
#  ifndef SLICER_USE_DEBUG_FILE
#    error if SLICER_DEBUG_MAIN or SLICER_BENCHMARK is defined, SLICER_USE_DEBUG_FILE must be defined
#  endif
#endif

#ifdef SLICER_USE_DEBUG_FILE
#  define CLOSE_DEBUG_FILE {fclose(STDERR);}
#else
#  define CLOSE_DEBUG_FILE {}
#endif

#ifdef SLICER_DEBUG_MAIN
#  define DEBUGPRINTF(...) {fprintf(STDERR, __VA_ARGS__); fflush(STDERR);}
#else
#  define DEBUGPRINTF(...) {}
#endif

#ifdef SLICER_BENCHMARK 
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
  READ_BINARY_NAIVE(&command, sizeof(int64), 1, input);
  return command;
}


int main(int argc, char **argv) {

#ifdef SLICER_BENCHMARK
  // ticks per second
  LARGE_INTEGER frequency;
  // ticks
  LARGE_INTEGER t[10], tt[10];
  double elapsedTime;
  // get ticks per second
  QueryPerformanceFrequency(&frequency);
  // start timer
  BENCHGETTICK(t[0]);
#endif  

  FILE * output = stdout;
  FILE * input  = stdin;
  char *filenameinput, *filenameoutput;
  
#ifdef BUFSIZE  
  char * buffer_in  = new char[BUFSIZE];
  char * buffer_out = new char[BUFSIZE];
  setvbuf(input,  buffer_in,  _IOFBF, BUFSIZE);
  setvbuf(output, buffer_out, _IOFBF, BUFSIZE);
#  define CLEAR_BUFFERS {delete buffer_in; delete buffer_out;}
#else
#  define CLEAR_BUFFERS 
#endif
  
  
  int minNumArgs = 4;
#ifdef SLICER_USE_DEBUG_FILE
  ++minNumArgs;
#endif
  
  if (argc<minNumArgs) {
    fprintf(stderr, "%d IS AN INCORRECT NUMBER OF ARGUMENTS!!!", argc-1);
    CLEAR_BUFFERS;
    return -1;
  }
  
  int argidx = 1;
  
#ifdef SLICER_USE_DEBUG_FILE
  const char *debugfilename = argv[argidx++];
  
  FILE * STDERR = fopen(debugfilename, "w");
  if (STDERR==NULL) {
    fprintf(stderr, "Could not open debug output file!!!!\n");
    return -1;
  }
  setConfessErrOutput(STDERR);

  DEBUGPRINTF("BEFORE EVERYTHING\n");
#endif
  
  const char * repairMode  = argv[argidx++];
  
  bool justRepair = strcmp("justrepair", repairMode)==0;
  bool doRepair = strcmp("repair", repairMode)==0;
  
  bool doincremental=false;
  if (!justRepair) {
    doincremental = strcmp("incremental", argv[argidx++])==0;
  }
  filenameinput = argv[argidx++];
  if (justRepair) {
    filenameoutput = argv[argidx++];
  }
  
  DEBUGPRINTF("BEFORE BINARIZING IO\n");

#ifdef INWINDOWS  
  _setmode(0, _O_BINARY);
  _setmode(1, _O_BINARY);
#endif

  Slic3r::TriangleMesh *mesh = new Slic3r::TriangleMesh();
#ifdef SLICER_USE_DEBUG_FILE
  mesh->stl.err = STDERR;
#else
  mesh->stl.err = stderr;
#endif
  
  BENCHGETTICK(t[1]);
  
  DEBUGPRINTF("BEFORE READ STL, doRepair==%d, justRepair==%d, file=%s\n", doRepair, justRepair, filenameinput)

  mesh->ReadSTLFile(filenameinput);

  DEBUGPRINTF("AFTER READ STL\n");

  BENCHGETTICK(t[2]);

#ifdef ENSURE_REPAIR
  if (justRepair || doRepair || mesh->needed_repair()) {
#else
  if (justRepair || doRepair) {
#endif
    DEBUGPRINTF("BEFORE REPAIRING\n");
    
    mesh->repair();
    
    DEBUGPRINTF("AFTER REPAIRING\n");
  }
  
  BENCHGETTICK(t[3]);
  
  if (justRepair) {
    DEBUGPRINTF("BEFORE WRITING STL\n");
    
    mesh->write_binary(filenameoutput);
    
    DEBUGPRINTF("AFTER WRITING STL\n");
    
    BENCHGETTICK(t[4]);
    
    BENCHPRINTF("\n\nBENCHMARK:\n");
    SHOWBENCHMARK("  SETUP         TIME: %f\n", t[0], t[1]);
    SHOWBENCHMARK("  STL READING   TIME: %f\n", t[1], t[2]);
    SHOWBENCHMARK("  STL REPAIRING TIME: %f\n", t[2], t[3]);
    SHOWBENCHMARK("  STL WRITING   TIME: %f\n", t[3], t[4]);
    
    CLOSE_DEBUG_FILE;
    CLEAR_BUFFERS;
    return 0;
  }
  
  if(!doRepair) {
    int64 needed_repair = mesh->needed_repair();
    
    DEBUGPRINTF("BEFORE NEED_REPAIR=%ld\n", needed_repair);
    
    WRITE_BINARY_NAIVE(&needed_repair, sizeof(double), 1, output);
    
    if (needed_repair) {
      BENCHGETTICK(t[4]);
      BENCHPRINTF("\n\nTERMINATING BECAUSE OF needed_repair. BENCHMARK:\n");
      SHOWBENCHMARK("  SETUP             TIME: %f\n", t[0], t[1]);
      SHOWBENCHMARK("  STL READING       TIME: %f\n", t[1], t[2]);
      SHOWBENCHMARK("  STL REPAIRING     TIME: %f\n", t[2], t[3]);
      SHOWBENCHMARK("  I/O AND FINISHING TIME: %f\n", t[3], t[4]);
      CLOSE_DEBUG_FILE;
      CLEAR_BUFFERS;
      return -1;
    }
    
    DEBUGPRINTF("AFTER NEED_REPAIR\n");
  }
  
  Slic3r::BoundingBoxf3 bb = mesh->bounding_box();
  
  double minz = bb.min.z;
  double maxz = bb.max.z;

  DEBUGPRINTF("BEFORE MAXZ=%f, MINZ=%f\n", maxz, minz);
  
  WRITE_BINARY_NAIVE(&minz, sizeof(double), 1, output);
  WRITE_BINARY_NAIVE(&maxz, sizeof(double), 1, output);
  fflush(output);

  DEBUGPRINTF("AFTER MAXZ, MINZ\n");
      
  int64 numslices, command;
  
  READ_BINARY_NAIVE(&numslices, sizeof(int64), 1, input);

  DEBUGPRINTF("AFTER NUMSLICES=%ld\n", numslices);
      
  std::vector<float>  zsf(numslices);
  std::vector<double> zsd(numslices);

  READ_BINARY_NAIVE(&(zsd[0]), sizeof(double), numslices, input);
  
  DEBUGPRINTF("AFTER ZS\n");
      
  for (int64 k = 0; k < numslices; ++k) {
    zsf[k] = (float)zsd[k];
    //DEBUGPRINTF("  %f\n", zsd[k]);
  }
  
  BENCHGETTICK(t[4]);

  Slic3r::TriangleMeshSlicer *slicer = new Slic3r::TriangleMeshSlicer(mesh, 0.0);
  std::vector<Slic3r::ExPolygons> expolygonss;
  int ret = 0;
  IOPaths iop(output);
  
  if (!doincremental) {
    if (zsf.size()>0) {
      slicer->slice(zsf, &expolygonss);
    }  
      //get rid of this to free memory
      delete mesh; 
      delete slicer;
      

      DEBUGPRINTF("AFTER SLICING\n");

      BENCHGETTICK(t[5]);
      
      std::vector<clp::Paths> pathss(expolygonss.size());
    if (zsf.size()>0) {
      std::vector<clp::Paths>::iterator paths = pathss.begin();
      for (std::vector<Slic3r::ExPolygons>::iterator expolygons = expolygonss.begin(); expolygons != expolygonss.end(); ++expolygons) {
        for (Slic3r::ExPolygons::iterator expolygon = expolygons->begin(); expolygon != expolygons->end(); ++expolygon) {
          Slic3r::Add_Slic3rExPolygon_to_ClipperPaths(*expolygon, &(*paths));
          *expolygon = ExPolygon(); //get rid of this to free memory
        }
        ++paths;
      }
      expolygonss = std::vector<Slic3r::ExPolygons>(); //get rid of this to free memory
      //int64 nbytes = io_num_bytes_total(pathss, PathOpen);
      //WRITE_BINARY_NAIVE(&nbytes, sizeof(int64), 1, output);
    }

      BENCHGETTICK(t[6]);

      //while ((command=getCommand(input)) >= 0) {
      for (size_t command = 0; command<pathss.size(); ++command) {
        DEBUGPRINTF("BEFORE COMMAND %ld\n", command);
          
        /*if (command>=pathss.size()) {
          ret = -1;
          break;
        }*/
        
        //BENCHGETTICK(tt[0]);
        
        //int64 nbytes = io_num_bytes(pathss[command], PathOpen);
        //WRITE_BINARY_NAIVE(&nbytes, sizeof(int64), 1, output);

        
        if (!iop.writePrefixedClipperPaths(pathss[command], PathOpen)) {
            DEBUGPRINTF("ERROR writing to the output in incremental mode!!! Error message <%s> in %s\n", iop.errs[0].message, iop.errs[0].function);
            ret = -1;
            break;
        }
        //BENCHGETTICK(tt[1]);
        //fflush(output);
        ////BENCHGETTICK(tt[2]);
        
        DEBUGPRINTF("\nAFTER COMMAND %ld\n", command);
        
        //BENCHPRINTF("\nFOR COMMAND %ld, TIMES\n", command);
        //SHOWBENCHMARK("  WRITING  SLICE TIME: %f\n", tt[0], tt[1]);
        ////SHOWBENCHMARK("  FLUSHING SLICE TIME: %f\n", tt[1], tt[2]);
        
        ////BENCHGETTICK(tt[3]);
        ////SHOWBENCHMARK("  WRITING THE ABOVE: %f\n", tt[2], tt[3]);
      }
  } else {
    
      std::vector<float>  zsf1(1);
      clp::Paths paths;
      expolygonss.reserve(1);

      //while ((command=getCommand(input)) >= 0) {
      for (size_t command = 0; command<zsf.size(); ++command) {
        DEBUGPRINTF("BEFORE COMMAND %ld\n", command);

        BENCHGETTICK(tt[0]);
        zsf1[0] = zsf[command];
        slicer->slice(zsf1, &expolygonss);
        
        for (Slic3r::ExPolygons::iterator expolygon = expolygonss[0].begin(); expolygon != expolygonss[0].end(); ++expolygon) {
          Slic3r::Add_Slic3rExPolygon_to_ClipperPaths(*expolygon, &paths);
          *expolygon = ExPolygon(); //get rid of this to free memory
        }
        expolygonss.clear();
        
        
        /*if (command>=pathss.size()) {
          ret = -1;
          break;
        }*/
        
        BENCHGETTICK(tt[1]);
        //int64 nbytes = io_num_bytes(pathss[command], PathOpen);
        //WRITE_BINARY_NAIVE(&nbytes, sizeof(int64), 1, output);
        if (!iop.writePrefixedClipperPaths(paths, PathOpen)) {
            DEBUGPRINTF("ERROR writing to the output!!! Error message <%s> in %s\n", iop.errs[0].message, iop.errs[0].function);
            ret = -1;
            break;
        }
        paths.clear();
        BENCHGETTICK(tt[2]);
        //fflush(output);
        //BENCHGETTICK(tt[2]);
        
        DEBUGPRINTF("\nAFTER COMMAND %ld\n", command);
        
        BENCHPRINTF("\nFOR COMMAND %ld, TIMES\n", command);
        SHOWBENCHMARK("  SLICING & CONVERSION TIME: %f\n", tt[0], tt[1]);
        SHOWBENCHMARK("  WRITING              TIME: %f\n", tt[1], tt[2]);
        //SHOWBENCHMARK("  FLUSHING SLICE TIME: %f\n", tt[1], tt[2]);
        
        //BENCHGETTICK(tt[3]);
        //SHOWBENCHMARK("  WRITING THE ABOVE: %f\n", tt[2], tt[3]);
      }
      //get rid of this to free memory
      delete mesh; 
      delete slicer;

  }
  
  DEBUGPRINTF("FINISHING, command=%ld\n", command);

  BENCHGETTICK(t[7]);
      
  BENCHPRINTF("\n\nBENCHMARK:\n");
  SHOWBENCHMARK(  "  SETUP         TIME: %f\n", t[0], t[1]);
  SHOWBENCHMARK(  "  STL READING   TIME: %f\n", t[1], t[2]);
  SHOWBENCHMARK(  "  STL REPAIRING TIME: %f\n", t[2], t[3]);
  SHOWBENCHMARK(  "  Z VALUES I/O  TIME: %f\n", t[3], t[4]);
  if (!doincremental) {
    SHOWBENCHMARK("  STL SLICING   TIME: %f\n", t[4], t[5]);
    SHOWBENCHMARK("  TYPE ADAPTING TIME: %f\n", t[5], t[6]);
    SHOWBENCHMARK("  MAIN I/O      TIME: %f\n", t[6], t[7]);
  } else {
    SHOWBENCHMARK("  SLICING & MAIN I/O TIME: %f\n", t[4], t[7]);
  }
  BENCHGETTICK(t[8]);
  SHOWBENCHMARK("  TOTAL         TIME: %f\n", t[0], t[8]);
  CLOSE_DEBUG_FILE;
  CLEAR_BUFFERS;
  return ret;
  

}
